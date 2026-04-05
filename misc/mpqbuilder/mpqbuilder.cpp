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
#include <boost/filesystem.hpp>

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

void renderProgressBar(const std::string& targetFile, size_t current, size_t total)
{
    const int barWidth = 40;

    double progress = total == 0 ? 0.0 : static_cast<double>(current) / total;
    int pos = static_cast<int>(barWidth * progress);

    std::cout << "\r\033[K" << targetFile << " ["; // \r = return, \033[K = clear line

    for (int i = 0; i < barWidth; ++i)
    {
        if (i < pos) std::cout << "#";
        else if (i == pos) std::cout << ">";
        else std::cout << "-";
    }

    int percent = static_cast<int>(progress * 100.0);

    std::cout << "] "
              << percent << "% ("
              << current << "/" << total << ")"
              << std::flush;
}

std::string normalizeArchivePath(const std::string& input)
{
    std::string normalized = input;
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    while (!normalized.empty() && (normalized.front() == '\\' || normalized.front() == '/'))
    {
        normalized.erase(0, 1);
    }
    return normalized;
}

bool isSafeArchivePath(const std::string& archivePath)
{
    if (archivePath.empty())
    {
        return false;
    }

    if (archivePath.find(':') != std::string::npos)
    {
        return false;
    }

    boost::filesystem::path archiveFsPath(archivePath);
    for (const auto& part : archiveFsPath)
    {
        const auto segment = part.string();
        if (segment == "..")
        {
            return false;
        }
    }

    return true;
}

bool isGeneratedInternalFile(const std::string& archivePath)
{
    return archivePath == "(attributes)" || archivePath == "(listfile)";
}

std::string manifestFileName()
{
    return ".tswow-manifest.txt";
}

int extractArchive(const std::string& archivePath, const std::string& outputDir)
{
    HANDLE archiveHandle = NULL;
    if (!SFileOpenArchive(archivePath.c_str(), 0, STREAM_FLAG_READ_ONLY, &archiveHandle))
    {
        std::cerr << "Failed to open archive " << archivePath << ", error=" << GetLastError() << "\n";
        return -1;
    }

    boost::filesystem::create_directories(outputDir);

    SFILE_FIND_DATA findData = {};
    HANDLE findHandle = SFileFindFirstFile(archiveHandle, "*", &findData, NULL);
    if (findHandle == NULL)
    {
        const auto error = GetLastError();
        std::cerr
            << "Failed to enumerate files in archive " << archivePath
            << ", error=" << error
            << ". The archive may be missing a usable (listfile).\n";
        SFileCloseArchive(archiveHandle);
        return -1;
    }

    std::vector<std::string> files;
    do
    {
        const std::string archiveFile = normalizeArchivePath(findData.cFileName);
        if (archiveFile.empty())
        {
            continue;
        }
        if (isGeneratedInternalFile(archiveFile))
        {
            continue;
        }
        files.push_back(archiveFile);
    }
    while (SFileFindNextFile(findHandle, &findData));

    SFileFindClose(findHandle);
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());

    const size_t totalFiles = files.size();
    const size_t reportEvery = progressInterval(totalFiles);
    size_t processedFiles = 0;
    size_t skippedFiles = 0;
    std::vector<std::string> extractedFiles;
    renderProgressBar(outputDir, processedFiles, totalFiles);

    for (const auto& archivedFile : files)
    {
        if (!isSafeArchivePath(archivedFile))
        {
            std::cerr << "Skipping unsafe archive path " << archivedFile << "\n";
            continue;
        }

        boost::filesystem::path destinationPath = boost::filesystem::path(outputDir) / boost::filesystem::path(archivedFile);
        boost::filesystem::create_directories(destinationPath.parent_path());

        HANDLE fileHandle = NULL;
        if (!SFileOpenFileEx(archiveHandle, archivedFile.c_str(), SFILE_OPEN_FROM_MPQ, &fileHandle))
        {
            std::cerr
                << "Warning: failed to open file in MPQ, skipping:"
                << " archive=" << archivePath
                << " file=" << archivedFile
                << " error=" << GetLastError()
                << "\n";
            ++skippedFiles;
            continue;
        }

        DWORD highSize = 0;
        DWORD lowSize = SFileGetFileSize(fileHandle, &highSize);
        if (lowSize == SFILE_INVALID_SIZE && GetLastError() != ERROR_SUCCESS)
        {
            std::cerr
                << "Warning: failed to read file size from MPQ, skipping:"
                << " archive=" << archivePath
                << " file=" << archivedFile
                << " error=" << GetLastError()
                << "\n";
            SFileCloseFile(fileHandle);
            ++skippedFiles;
            continue;
        }

        std::ofstream output(destinationPath.string(), std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            std::cerr << "Failed to create output file " << destinationPath.string() << "\n";
            SFileCloseFile(fileHandle);
            SFileCloseArchive(archiveHandle);
            return -1;
        }

        bool readFailed = false;
        std::vector<char> buffer(1024 * 1024);
        while (true)
        {
            DWORD bytesRead = 0;
            if (!SFileReadFile(fileHandle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, NULL))
            {
                const auto error = GetLastError();
                if (error != ERROR_HANDLE_EOF)
                {
                    std::cerr
                        << "Warning: failed to read file contents from MPQ, skipping:"
                        << " archive=" << archivePath
                        << " file=" << archivedFile
                        << " error=" << error
                        << "\n";
                    readFailed = true;
                    break;
                }
            }

            if (bytesRead == 0)
            {
                break;
            }

            output.write(buffer.data(), bytesRead);
            if (!output.good())
            {
                std::cerr << "Failed to write extracted file " << destinationPath.string() << "\n";
                output.close();
                SFileCloseFile(fileHandle);
                SFileCloseArchive(archiveHandle);
                return -1;
            }
        }

        output.close();
        SFileCloseFile(fileHandle);
        if (readFailed)
        {
            remove(destinationPath.string().c_str());
            ++skippedFiles;
            continue;
        }
        extractedFiles.push_back(archivedFile);

        ++processedFiles;
        if (processedFiles == totalFiles || processedFiles % reportEvery == 0)
        {
            renderProgressBar(outputDir, processedFiles, totalFiles);
        }
    }

    const boost::filesystem::path manifestPath = boost::filesystem::path(outputDir) / manifestFileName();
    std::ofstream manifest(manifestPath.string(), std::ios::trunc);
    if (!manifest.is_open())
    {
        std::cerr << "Failed to create extraction manifest " << manifestPath.string() << "\n";
        SFileCloseArchive(archiveHandle);
        return -1;
    }

    for (const auto& extractedFile : extractedFiles)
    {
        manifest << extractedFile << "\n";
    }
    manifest.close();

    if (skippedFiles > 0)
    {
        std::cout << "Skipped unreadable files: " << skippedFiles << "\n";
    }

    renderProgressBar(outputDir, totalFiles, totalFiles);
    std::cout << std::endl;

    SFileCloseArchive(archiveHandle);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && std::string(argv[1]) == "extract")
    {
        if (argc < 4)
        {
            std::cout << "Usage: mpqbuilder extract inputfile outputdir";
            return -1;
        }

        return extractArchive(argv[2], argv[3]);
    }

    if (argc < 3)
    {
        std::cout << "Usage: mpqbuilder filelist outputfile [compression]\n";
        std::cout << "   or: mpqbuilder extract inputfile outputdir";
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
    size_t power    = 4;
    auto outputfile = temp.m_file.c_str();
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
    renderProgressBar(outputfile, processedFiles, totalFiles);

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
            renderProgressBar(outputfile, processedFiles, totalFiles);
        }
    }
    SFileFlushArchive(handle);
    SFileCloseArchive(handle);

    renderProgressBar(outputfile, totalFiles, totalFiles);
    std::cout << std::endl;

    // 3. Save to real output file
    std::string outputFile = argv[2];
    if (!clearFile(outputFile, "Failed to remove old mpq file ")) return -1;

    std::ifstream  src(temp.m_file, std::ios::binary);
    std::ofstream  dst(outputFile, std::ios::binary);
    dst << src.rdbuf();
    return 0;
}
