#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/ConfigurationGroup.h>

#include <Magnum/Trade/AbstractImporter.h>

using namespace Magnum;

int main() {

{
PluginManager::Manager<Trade::AbstractImporter> manager;
/* [basis-target-format-suffix] */
    /* Choose Etc2 target format */
    Containers::Pointer<Trade::AbstractImporter> importerEtc2 =
        manager.instantiate("BasisImporterEtc2");

    /* Choose Bc5 target format */
    Containers::Pointer<Trade::AbstractImporter> importerBc5 =
        manager.instantiate("BasisImporterBc5");
/* [basis-target-format-suffix] */
}

{
PluginManager::Manager<Trade::AbstractImporter> manager;
/* [basis-target-format-config] */
    /* Chooses default Etc2 target format */
    Containers::Pointer<Trade::AbstractImporter> importer =
        manager.instantiate("BasisImporter");

    /* Change to Bc5 */
    importer->configuration().setValue("format", "Bc5");
/* [basis-target-format-config] */
}

}
