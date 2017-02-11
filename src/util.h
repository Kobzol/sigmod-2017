#pragma once

#include <string>

std::string find_prefix(const std::string& word);
std::string find_prefix_from(const std::string& word, size_t from = 0);
size_t find_prefix_length(const std::string& word, size_t start_index = 0);
