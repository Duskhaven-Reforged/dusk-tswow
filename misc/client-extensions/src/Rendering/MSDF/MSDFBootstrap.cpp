#include "MSDFBootstrap.h"

#include "D3D.h"
#include "MSDF.h"

void MSDFBootstrap::initialize() {
    D3D::initialize();
    MSDF::initialize();
}
