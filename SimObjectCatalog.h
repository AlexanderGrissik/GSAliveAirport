#pragma once

#include <string>
#include <vector>

namespace parking_services
{
// The raw, enumerated SimObject/livery catalog. Pure data: it holds the entries
// collected by a GSReqCatalog (executed on the SimConnect thread) and offers
// lookups. It performs no SimConnect I/O itself. Owned solely by
// GroundServicesConfig, and only ever reached through it.
class SimObjectCatalog final
{
  public:
    struct Entry
    {
        std::string title;
        std::string livery;
    };

    void Adopt(std::vector<Entry> entries);
    void Reset();
    [[nodiscard]] const std::vector<Entry> &Entries() const { return m_entries; }

  private:
    std::vector<Entry> m_entries;
};
} // namespace parking_services
