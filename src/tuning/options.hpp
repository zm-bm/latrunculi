#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

#include "search/parameters.hpp"

namespace tuning {

// Unknown names return false. Invalid values throw without changing the settings.
bool set(search::Parameters&, std::string_view name, std::string_view value);
bool valid(const search::Parameters&) noexcept;
std::string uci_options();
void export_parameters(std::ostream&);

} // namespace tuning
