
#include <string>
#include <stdexcept>

#include "json_builder.h"

using namespace std;

namespace json
{
    Builder::Builder() : after_key_ptr_(new AfterKeyHandler(this)),
                         key_enddict_ptr_(new KeyEndDictHandler(this)),
                         after_startarray_ptr_(new AfterStartArrayHandler(this))
    {
        nodes_stack_.push_back(new NodeStruct);
    }

    AfterKeyHandler& Builder::Key(const string& key)
    {
        NodeStruct& ns = *nodes_stack_.back();
        if (ns.node_open_status != WhatOpened::OPEN_DICT)
            throw logic_error("Key : key setting not in dictionary");
        if (ns.is_key_saved)
            throw logic_error("Key : key redefinition");
        ns.save_key = key;
        ns.is_key_saved = true;
        // По правилу 1 после Key() доступны только Value(), StartDict() и StartArray() - класс AfterKeyHandler
        return *after_key_ptr_;
    }

    AfterStartArrayHandler& Builder::StartArray()
    {
        CheckDocumentPrepare(false, "StartArray"s);
        NodeStruct& ns_new = CheckWhenCreateNewStruct(WhatOpened::OPEN_ARRAY);
        ns_new.node = Node(Array{});
        return *after_startarray_ptr_;
    }

    Builder& Builder::EndArray()
    {
        EndStructure(WhatOpened::OPEN_ARRAY);
        return *this;
    }

    KeyEndDictHandler& Builder::StartDict()
    {
        CheckDocumentPrepare(false, "StartDict"s);
        NodeStruct& ns_new = CheckWhenCreateNewStruct(WhatOpened::OPEN_DICT);
        ns_new.node = Node(Dict{});
        return *key_enddict_ptr_; // По правилу 3 после StartDict() возможны только Key() и EndDict()
    }

    Builder& Builder::EndDict()
    {
        EndStructure(WhatOpened::OPEN_DICT);
        return *this;
    }

    Node Builder::Build()
    {
        CheckDocumentPrepare(true, "Build"s);
        return (*nodes_stack_.back()).node;
    }

    Builder::~Builder()
    {
        for (auto ns_ptr : nodes_stack_)
            delete ns_ptr;
        delete after_key_ptr_;
        delete key_enddict_ptr_;
        delete after_startarray_ptr_;
    }

    void Builder::CheckDocumentPrepare(bool is_check_for_ready, const string& check_name)
    {
        NodeStruct& ns = *nodes_stack_.back();
        if (nodes_stack_.size() > 1 || ns.node_open_status != WhatOpened::OPEN_NOTHING
            || !ns.is_node_has_value)
        { // Документ ещё не подготовлен
            if (is_check_for_ready)
                throw logic_error(check_name + " : document not ready"s);
        }
        else // Документ уже полностью сформирован
        {
            if (!is_check_for_ready)
                throw logic_error(check_name + " : document already prepared"s);
        }
    }

    Builder::NodeStruct& Builder::CheckWhenCreateNewStruct(WhatOpened what)
    {
        static const char* throw_text[] = {"StartArray : array in dictionary - the key was not set",
                                           "StartArray : already has value",
                                           "StartDict : nested dictionaries - the key was not set",
                                           "StartDict : already has value"
                                          };
        int msg_num = (static_cast<int>(what) - static_cast<int>(WhatOpened::OPEN_ARRAY)) * 2;
        NodeStruct& ns = *nodes_stack_.back();
        if (ns.node_open_status == WhatOpened::OPEN_DICT)
        {
            if (!ns.is_key_saved)
                throw logic_error(throw_text[msg_num]);
        }
        else if (ns.node_open_status == WhatOpened::OPEN_NOTHING)
        {
            if (ns.is_node_has_value)
                throw logic_error(throw_text[msg_num + 1]);
        }
        nodes_stack_.push_back(new NodeStruct);
        NodeStruct& ns_new = *nodes_stack_.back();
        ns_new.node_open_status = what;
        return ns_new;
    }

    void Builder::EndStructure(WhatOpened what)
    {

        static const char* throw_text[] = {"EndArray : not in array",
                                           "EndArray : array in dictionary - the key was not set",
                                           "EndArray : already has value",
                                           "EndDict : not in dictionary",
                                           "EndDict : nested dictionaries - the key was not set",
                                           "EndDict : already has value"
                                          };

        int msg_num = (static_cast<int>(what) - static_cast<int>(WhatOpened::OPEN_ARRAY)) * 3;

        NodeStruct ns_struct = move(*nodes_stack_.back());
        if (ns_struct.node_open_status != what)
            throw logic_error(throw_text[msg_num]);

        delete nodes_stack_.back();
        nodes_stack_.pop_back();

        NodeStruct& ns = *nodes_stack_.back();
        if (ns.node_open_status == WhatOpened::OPEN_ARRAY)
        {
            const_cast<Array&>(ns.node.AsArray()).push_back(move(ns_struct.node));
        }
        else if (ns.node_open_status == WhatOpened::OPEN_DICT)
        {
            if (!ns.is_key_saved)
                throw logic_error(throw_text[msg_num + 1]);
            const_cast<Dict&>(ns.node.AsDict())[ns.save_key] = move(ns_struct.node);
            ns.is_key_saved = false;
        }
        else
        {
            if (!ns.is_node_has_value)
                ns.node = move(ns_struct.node);
            else
                throw logic_error(throw_text[msg_num + 2]);
        }
        ns.is_node_has_value = true;
    }

    Builder& Builder::ValueHelper(Node value)
    {
        CheckDocumentPrepare(false, "Value");
        NodeStruct& ns = *nodes_stack_.back();
        if (ns.node_open_status == WhatOpened::OPEN_ARRAY)
        {
            const_cast<Array&>(ns.node.AsArray()).push_back(std::move(value));
        }
        else if (ns.node_open_status == WhatOpened::OPEN_DICT)
        {
            if (!ns.is_key_saved)
                throw std::logic_error("Value : the key was not set");
            const_cast<Dict&>(ns.node.AsDict())[ns.save_key] = std::move(value);
            ns.is_key_saved = false;
        }
        else
        {
            if (!ns.is_node_has_value)
            {
                ns.node = std::move(value);
                ns.is_node_has_value = true;
            }
            else
            {
                throw std::logic_error("Value : already has value");
            }
        }
        return *this;
    }
} //namespace json
