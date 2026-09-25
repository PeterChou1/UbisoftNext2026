#include "GameComponents.h"

#include "Reflection/ComponentCatalog.h"

void RegisterGameComponents()
{
    ComponentCatalog& catalog = ComponentCatalog::Get();
    catalog.Register<Health>(ComponentNames::Health, "Hit points, removed by DamageZone");
    catalog.Register<Faction>(ComponentNames::Faction, "Side, name and colour");
    catalog.Register<Waypoint>(ComponentNames::Waypoint, "Path point for WaypointFollower");
}
