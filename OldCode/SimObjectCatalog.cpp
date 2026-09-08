#include "SimObjectCatalog.h"

#include <utility>

namespace parking_services
{
void SimObjectCatalog::Adopt(std::vector<Entry> entries)
{
    m_entries = std::move(entries);
}

void SimObjectCatalog::Reset()
{
    m_entries.clear();
}
} // namespace parking_services
