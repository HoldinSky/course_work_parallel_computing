#include "file_indexer.h"
#include "text_utils/file_parser.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

// utilities

void FileIndexer::addMapping(const std::string& word, const std::string& path)
{
    std::set<std::string> pathSet;
    try
    {
        pathSet = this->index.at(word);
        pathSet.insert(path);
    }
    catch (const std::out_of_range& _)
    {
        pathSet.insert(path);
    }
    this->allFilePaths.insert(path);

    this->index.insert_or_assign(word, pathSet);
}

void FileIndexer::addMapping(const std::string& word, const std::set<std::string>& paths)
{
    std::set<std::string> pathSet = paths;
    try
    {
        pathSet = this->index.at(word);
        for (const auto& path : paths)
        {
            pathSet.insert(path);
            this->allFilePaths.insert(path);
        }
    }
    catch (const std::out_of_range& _)
    {
    }

    this->index.insert_or_assign(word, pathSet);
}

void FileIndexer::removeMapping(std::string const& word, std::string const& path)
{
    try
    {
        std::set<std::string> pathSet = this->index.at(word);
        pathSet.erase(path);
        if (path.empty())
        {
            this->index.erase(word);
        }
    }
    catch (std::out_of_range const& _)
    {
    }
}

// private here

int FileIndexer::indexDirectory(fs::path const& path)
{
    for (const auto& entry : fs::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            this->indexDirectory(entry.path().string());
        }
        else
        {
            this->indexFile(entry.path().string());
        }
    }

    return 0;
}

int FileIndexer::removeDirectory(fs::path const& path)
{
    for (auto const& entry : fs::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            this->removeDirectory(entry.path().string());
        }
        else
        {
            this->removeFile(entry.path().string());
        }
    }

    return 0;
}

int FileIndexer::indexFile(fs::path const& path)
{
    auto const& pathStr = path.string();
    std::ifstream file(pathStr, std::ios::in);
    if (!file)
    {
        fprintf(stderr, "[ERR] Could not open file %ls\n", path.c_str());
        return ERROR_FILE_CANNOT_BE_OPENED;
    }

    parseInputStreamByWord(file, [&](char const* word) { this->indexWord(word, pathStr); });

    return 0;
}

int FileIndexer::removeFile(fs::path const& path)
{
    auto const& pathStr = path.string();
    std::ifstream file(pathStr, std::ios::in);
    if (!file)
    {
        fprintf(stderr, "[ERR] Could not open file %ls\n", path.c_str());
        return ERROR_FILE_CANNOT_BE_OPENED;
    }

    parseInputStreamByWord(file, [&](char const* word) { this->removeWord(word, pathStr); });

    return 0;
}

void FileIndexer::indexWord(const char* word, const std::string& path)
{
    const auto wordStr = std::string(word);
    this->addMapping(wordStr, path);
}

void FileIndexer::removeWord(char const* word, std::string const& path)
{
    const auto wordStr = std::string(word);
    this->removeMapping(wordStr, path);
}

std::set<std::string> FileIndexer::findFiles(const std::string& word)
{
    try
    {
        return this->index.at(word);
    }
    catch (const std::out_of_range& _)
    {
        return {};
    }
}

void FileIndexer::readIndexFromCSV()
{
    if (!fs::exists(defaultIndexFile))
    {
        return;
    }

    std::ifstream file(defaultIndexFile, std::ios::in);
    if (!file || !file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string cell;
        std::set<std::string> storedPaths;

        std::getline(ss, cell, ',');
        const auto word = cell;

        while (std::getline(ss, cell, ','))
        {
            storedPaths.insert(cell);
        }

        this->addMapping(word, storedPaths);
    }
}

void FileIndexer::saveIndexToCSV()
{
    const bool exists = fs::exists(defaultIndexFile);
    if (!this->overwriteSave && exists)
    {
        return;
    }

    if (!exists)
    {
        fs::path targetFilePath(defaultIndexFile);

        if (targetFilePath.has_parent_path())
        {
            fs::create_directories(targetFilePath.parent_path());
        }
    }

    std::ofstream file(defaultIndexFile, std::ios::out);
    if (!file || !file.is_open())
    {
        return;
    }

    std::string line;
    std::stringstream ss(line);
    for (const auto& [word, paths] : this->index)
    {
        ss << word;
        for (const auto& path : paths)
        {
            ss << ',' << path;
        }
        ss << '\n';
        file << ss.str();
        ss.str(std::string());
    }
}

// public here

int FileIndexer::addToIndex(const std::string& pathStr)
{
    auto const path = fs::path(pathStr);
    if (!fs::exists(path))
    {
        fprintf(stderr, "[ERR] Path %ls does not exist\n", path.c_str());
        return ERROR_PATH_DOES_NOT_EXIST;
    }

    exclusiveLock _(this->indexLock);
    int rcode;
    this->isCurrentlyIndexing.exchange(true);
    if (fs::is_directory(path))
    {
        rcode = this->indexDirectory(path);
    }
    else
    {
        if (this->allFilePaths.contains(pathStr))
        {
            return ERROR_FILE_ALREADY_INDEXED;
        }
        rcode = this->indexFile(path);
    }
    this->isCurrentlyIndexing.exchange(false);

    return rcode;
}

int FileIndexer::removeFromIndex(std::string const& pathStr)
{
    auto const path = fs::path(pathStr);
    if (!fs::exists(path))
    {
        fprintf(stderr, "[ERR] Path %ls does not exist\n", path.c_str());
        return ERROR_PATH_DOES_NOT_EXIST;
    }

    exclusiveLock _(this->indexLock);
    int rcode;
    if (fs::is_directory(path))
    {
        rcode = this->removeDirectory(path);
    }
    else
    {
        if (!this->allFilePaths.contains(pathStr))
        {
            return ERROR_FILE_ALREADY_NOT_INDEXED;
        }
        rcode = this->removeFile(path);
    }

    return rcode;
}

void FileIndexer::removeAllFromIndex()
{
    exclusiveLock _(this->indexLock);

    this->index.clear();
    this->allFilePaths.clear();
}

std::set<std::string> FileIndexer::reindexAll()
{
    exclusiveLock _(this->indexLock);
    this->isCurrentlyIndexing.exchange(true);

    this->index.clear();
    for (auto const& entry : this->allFilePaths)
    {
        auto const path = fs::path(entry);
        if (!fs::is_directory(path))
        {
            this->indexDirectory(path);
        }
        else
        {
            this->indexFile(path);
        }
    }

    this->isCurrentlyIndexing.exchange(false);

    return this->allFilePaths;
}


int FileIndexer::all(const std::vector<std::string>& words, std::set<std::string>* const out_Paths)
{
    if (this->isCurrentlyIndexing.load())
    {
        return ERROR_CURRENTLY_INDEXING;
    }

    sharedLock _(this->indexLock);
    if (words.empty())
    {
        return ERROR_WORDS_NOT_PROVIDED;
    }

    *out_Paths = this->findFiles(words[0]);
    std::set<std::string> intersection;
    for (uint64_t i = 1; i < words.size(); ++i)
    {
        const auto& set = this->findFiles(words[i]);

        std::set_intersection(
            out_Paths->begin(), out_Paths->end(),
            set.begin(), set.end(),
            std::inserter(intersection, intersection.begin())
        );
        *out_Paths = intersection;
        intersection.clear();
    }

    return 0;
}

int FileIndexer::any(const std::vector<std::string>& words, std::set<std::string>* const out_Paths)
{
    if (this->isCurrentlyIndexing.load())
    {
        return ERROR_CURRENTLY_INDEXING;
    }

    sharedLock _(this->indexLock);
    if (words.empty())
    {
        return ERROR_WORDS_NOT_PROVIDED;
    }

    for (const auto& word : words)
    {
        for (const auto& file : this->findFiles(word))
        {
            out_Paths->insert(file);
        }
    }

    return 0;
}