#include "ComponentCatalog.h"

ComponentCatalog& ComponentCatalog::Get()
{
    static ComponentCatalog catalog;
    return catalog;
}
