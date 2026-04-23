#include <UtilityClasses/NoCopy.hpp>
#include <Files/File.hpp>

namespace SF::Engine
{
    // lets try some new naming convention cuz raymond thinks its tough
    struct MapData
    {
        const char *m_Name;
        size_t m_Size;
        File m_MapFile;
    };

    class MapManager : NoCopy
    {
    private:
        MapManager();
        ~MapManager();

    public:
        void BrowseToURL(const char *name); // Can be a map location or server, either way calls load with some map data
    private:
        void Load(MapData &data); // Can't be null now :)
    };
}