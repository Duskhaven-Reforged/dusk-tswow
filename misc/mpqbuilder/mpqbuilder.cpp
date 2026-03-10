/*
 * This file is part of tswow (https://github.com/tswow)
 *
 * Copyright (C) 2020 tswow <https://github.com/tswow/>
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include <StormLib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cmath>

inline bool exists(std::string const& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

bool clearFile(std::string const& file, const char* errorMsg)
{
    if (exists(file))
    {
        remove(file.c_str());
        if (exists(file))
        {
            std::cout << errorMsg << file << "\n";
            return false;
        }
    }
    return true;
}

bool resolveCompression(std::string compressionName, DWORD& compressionMask)
{
    std::transform(
        compressionName.begin(),
        compressionName.end(),
        compressionName.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
    );

    if (compressionName.empty() || compressionName == "zlib")
    {
        compressionMask = MPQ_COMPRESSION_ZLIB;
        return true;
    }

    if (compressionName == "pkware" || compressionName == "pkzip")
    {
        compressionMask = MPQ_COMPRESSION_PKWARE;
        return true;
    }

    if (compressionName == "bzip2")
    {
        compressionMask = MPQ_COMPRESSION_BZIP2;
        return true;
    }

    if (compressionName == "sparse")
    {
        compressionMask = MPQ_COMPRESSION_SPARSE;
        return true;
    }

    if (compressionName == "lzma")
    {
        compressionMask = MPQ_COMPRESSION_LZMA;
        return true;
    }

    return false;
}

size_t progressInterval(size_t totalFiles)
{
    if (totalFiles <= 100)
    {
        return 1;
    }

    return std::max<size_t>(1, static_cast<size_t>(std::ceil(totalFiles / 100.0)));
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "Usage: mpqbuilder filelist outputfile [compression]";
        return -1;
    }

    DWORD compressionMask = MPQ_COMPRESSION_ZLIB;
    if (argc >= 4 && !resolveCompression(argv[3], compressionMask))
    {
        std::cerr
            << "Unknown compression type '" << argv[3] << "'. "
            << "Supported values: zlib, pkware, pkzip, bzip2, sparse, lzma\n";
        return -1;
    }

    // 1. Read input file
    std::string fileList = argv[1];
    if (!exists(fileList))
    {
       std::cerr << "File list " << fileList << " does not exist\n";
    }
    std::ifstream is(fileList);
    std::string line;
    std::vector<std::pair<std::string,std::string>> files;
    std::vector<std::string> errors;
    while (std::getline(is, line))
    {
        // Trim trailing \r (Windows) and whitespace so path inside MPQ is exact
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (line.empty())
            continue;
        size_t fst = line.find_first_of('\t');
        if (fst == std::string::npos)
        {
            errors.push_back("Malformed line:" + line);
            continue;
        }

        std::string src = line.substr(0, fst);
        std::string dst = line.substr(fst + 1);
        while (!src.empty() && (src.front() == ' ' || src.front() == '\t'))
            src.erase(0, 1);
        while (!dst.empty() && (dst.back() == '\r' || dst.back() == ' ' || dst.back() == '\t'))
            dst.pop_back();

        if (!exists(src))
        {
            errors.push_back("Missing file:" + src);
            continue;
        }
        files.push_back(std::make_pair(src,dst));
    }

    if (errors.size() > 0)
    {
        std::cerr << "Errors encountered when reading file list:\n";
        for (std::string const& error : errors)
        {
           std::cerr << error << "\n";
        }
        return -1;
    }

    if(files.size() == 0)
    {
        std::cerr << "Tried creating an MPQ with no files\n";
        return -1;
    }

    if (files.size() > 0x80000)
    {
        std::cerr << "Tried to create an MPQ with more than the maximum supported " << 0x80000 << " files\n";
        return -1;
    }

    // 2. Write MPQ
    struct TempFile {
       std::string m_file;
       TempFile(std::string const& file): m_file(file){}
       ~TempFile() {
           clearFile(m_file, "Failed to remove temp file ");
       }
    } temp(std::string(argv[2])+".temp");

    if (!clearFile(temp.m_file, "Failed to remove old temp file ")) return -1;

    HANDLE handle = NULL;
    size_t power = 4;
    while (power < files.size()) power <<= 1;
    if (!SFileCreateArchive(temp.m_file.c_str(), 0, power, &handle))
    {
        std::cerr << "Failed to create output mpq file " << temp.m_file << "\n";
        return -1;
    }

    if (!SFileSetDataCompression(compressionMask))
    {
        std::cerr << "Failed to configure MPQ compression, error=" << GetLastError() << "\n";
        SFileCloseArchive(handle);
        return -1;
    }

    const size_t totalFiles = files.size();
    const size_t reportEvery = progressInterval(totalFiles);
    size_t processedFiles = 0;
    std::cout << "Adding files to MPQ: 0/" << totalFiles << "\n";

    for (auto const& pair : files)
    {
        std::string archivedName = pair.second;
        std::replace(archivedName.begin(), archivedName.end(), '/', '\\');

        if (!SFileAddFile(handle, pair.first.c_str(), archivedName.c_str(), MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING))
        {
            std::cerr
                << "Failed to add file to MPQ:"
                << " src=" << pair.first
                << " dst=" << archivedName
                << " error=" << GetLastError()
                << "\n";
            SFileCloseArchive(handle);
            return -1;
        }

        ++processedFiles;
        if (processedFiles == totalFiles || processedFiles % reportEvery == 0)
        {
            std::cout << "Adding files to MPQ: " << processedFiles << "/" << totalFiles << "\n";
        }
    }
    SFileFlushArchive(handle);
    SFileCloseArchive(handle);

    // 3. Save to real output file
    std::string outputFile = argv[2];
    if (!clearFile(outputFile, "Failed to remove old mpq file ")) return -1;

    std::ifstream  src(temp.m_file, std::ios::binary);
    std::ofstream  dst(outputFile, std::ios::binary);
    dst << src.rdbuf();
    return 0;
}