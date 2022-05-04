#pragma once

#include <string>
#include <stdexcept>
#include "json.h"

namespace json
{
    class AfterKeyHandler;
    class KeyEndDictHandler;
    class AfterStartArrayHandler;

    class Builder
    {
    public:

        Builder();
        ~Builder();

        AfterStartArrayHandler& StartArray();
        Builder& EndArray();

        KeyEndDictHandler& StartDict();
        Builder& EndDict();
        AfterKeyHandler& Key(const std::string&);

        template <typename T>
        Builder& Value(T value)
        {
            return ValueHelper(Node(std::move(value)));
        }

        Node Build();

        friend class AfterKeyHandler;
        friend class KeyEndDictHandler;
        friend class AfterStartArrayHandler;

    private:
        enum class WhatOpened
        {
            OPEN_NOTHING = 0,
            OPEN_ARRAY,
            OPEN_DICT
        };

        struct NodeStruct
        {
            Node node;
            bool is_node_has_value = false;
            WhatOpened node_open_status = WhatOpened::OPEN_NOTHING;
            std::string save_key; //запомненный ключ создаваемой записи словаря
            bool is_key_saved = false;
        };

        std::vector<NodeStruct*> nodes_stack_; // стек указателей на вершины JSON, которые образуют строящийся документ
        AfterKeyHandler* after_key_ptr_;
        KeyEndDictHandler* key_enddict_ptr_;
        AfterStartArrayHandler* after_startarray_ptr_;
        // Приватные методы класса Builder
        NodeStruct& CheckWhenCreateNewStruct(WhatOpened what);
        void EndStructure(WhatOpened what);
        void CheckDocumentPrepare(bool is_check_for_ready, const std::string& check_name);
        Builder& ValueHelper(Node value);
    };

    class AfterKeyHandler
    { // Возвращается после Key
    public:
        AfterKeyHandler(Builder* builder_ptr) : builder_ptr_(builder_ptr)
        {}

        template <typename T>
        KeyEndDictHandler& Value(T value)
        { // Этот Value будет вызван только после Key(), а по правилу 2 после этого возможен только Key() или EndDict()
            builder_ptr_->Value(std::move(value));
            return *builder_ptr_->key_enddict_ptr_;
        }

        KeyEndDictHandler& StartDict()
        {
            builder_ptr_->StartDict();
            return *builder_ptr_->key_enddict_ptr_;
        }

        AfterStartArrayHandler& StartArray()
        {
            builder_ptr_->StartArray();
            return *builder_ptr_->after_startarray_ptr_;
        }

    private:
        Builder* builder_ptr_;
    };

    class KeyEndDictHandler
    {
    public:
        KeyEndDictHandler(Builder* builder_ptr) : builder_ptr_(builder_ptr)
        {}

        AfterKeyHandler& Key(const std::string& key)
        {
            builder_ptr_->Key(key);
            // По правилу 1 после Key() доступны только Value(), StartDict() и StartArray() - класс AfterKeyHandler
            return *builder_ptr_->after_key_ptr_;
        }

        Builder& EndDict()
        {
            builder_ptr_->EndDict();
            return *builder_ptr_;
        }

    private:
        Builder* builder_ptr_;
    };

    class AfterStartArrayHandler
    {
    public:
        AfterStartArrayHandler(Builder* builder_ptr) : builder_ptr_(builder_ptr)
        {}

        template <typename T>
        AfterStartArrayHandler& Value(T value)
        {
            builder_ptr_->Value(std::move(value));
            return *builder_ptr_->after_startarray_ptr_;
        }

        KeyEndDictHandler& StartDict()
        {
            builder_ptr_->StartDict();
            return *builder_ptr_->key_enddict_ptr_;
        }

        AfterStartArrayHandler& StartArray()
        {
            builder_ptr_->StartArray();
            return *builder_ptr_->after_startarray_ptr_;
        }

        Builder& EndArray()
        {
            builder_ptr_->EndArray();
            return *builder_ptr_;
        }

    private:
        Builder* builder_ptr_;
    };
} //namespace json
