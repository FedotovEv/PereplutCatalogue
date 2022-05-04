#include <fstream>
#include <iostream>
#include <string_view>
#include <filesystem>
#include <stdexcept>

#include "json_reader.h"
#include "transport_catalogue.h"

using namespace std;
using namespace std::filesystem;
using namespace std::literals;

enum class ErrCodes
{
    ERRCODE_NO_ERROR = 0,
    ERRCODE_BAD_COMMAND,
    ERRCODE_INPUT_FILE_NOT_FOUND,
    ERRCODE_INPUT_FILE_OPEN_ERROR,
    ERRCODE_OUTPUT_FILE_CREATE_ERROR,
    ERRCODE_TRANSPORT_CATALOGUE_REQUEST_ERROR,
    ERRCODE_UNKNOWN_ERROR
};

void PrintUsage(std::ostream& stream = std::cerr)
{
    stream << "Формат команды: transport_catalogue [make_base|process_requests] <входной_файл>\n"sv;
}

void ErrorCodeAnalize(ErrCodes err_code)
{
    const char* err_message[] = { "Ошибка в формате вызова программы",
                                 "Входной файл не найден",
                                 "Не удалось открыть входной файл",
                                 "Не удалось создать выходной файл",
                                 "Ошибка при исполнении запросов к транспортному справочнику",
                                 "Неизвестная ошибка" };

    int i = static_cast<int>(err_code);
    if (i)
    {
        cerr << err_message[i - 1] << endl;
        PrintUsage();
    }
    exit(i);
}

int main(int argc, char* argv[])
{
    ErrCodes err_code = ErrCodes::ERRCODE_NO_ERROR;

    if (argc != 3)
        ErrorCodeAnalize(ErrCodes::ERRCODE_BAD_COMMAND);

    const string_view mode(argv[1]);
    path infile_path(argv[2]);
    if (!exists(infile_path))
        ErrorCodeAnalize(ErrCodes::ERRCODE_INPUT_FILE_NOT_FOUND);
    ifstream ifile(infile_path.c_str());
    if (!ifile)
        ErrorCodeAnalize(ErrCodes::ERRCODE_INPUT_FILE_OPEN_ERROR);

    if (mode == "make_base"sv)
    {
        transport::TransportCatalogue trans_cat;
        try
        {
            transport::reader::JSONReader jsr(ifile, trans_cat);
            jsr.ProcessAddInfoRequests();
            jsr.BuildBusRouter();
            jsr.ProcessSerialize();
        }
        catch (const exception & exc)
        {
            cerr << exc.what() << endl;
            ErrorCodeAnalize(ErrCodes::ERRCODE_TRANSPORT_CATALOGUE_REQUEST_ERROR);
        }
    }
    else if (mode == "process_requests"sv)
    {
        ofstream ofile(infile_path.stem().concat(".out"sv).c_str());
        if (!ofile)
            ErrorCodeAnalize(ErrCodes::ERRCODE_OUTPUT_FILE_CREATE_ERROR);
        transport::TransportCatalogue trans_cat;
        try
        {
            transport::reader::JSONReader jsr(ifile, trans_cat);
            jsr.ProcessDeserialize();
            json::Document doc = jsr.ProcessGetInfoRequests();
            Print(doc, cout);
            Print(doc, ofile);
        }
        catch (const exception& exc)
        {
            cerr << exc.what() << endl;
            ErrorCodeAnalize(ErrCodes::ERRCODE_TRANSPORT_CATALOGUE_REQUEST_ERROR);
        }
    }
    else
    {
        ErrorCodeAnalize(ErrCodes::ERRCODE_BAD_COMMAND);
    }
}
