#include "CInterfaceElement.h"

#include "game/CEngine.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x18979c
CInterfaceElement::CInterfaceElement(CInterfaceMenu* menu, float x, float y)
    : mX(x), mY(y), mMenu(menu), mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x189808
CInterfaceElement::~CInterfaceElement() = default;

void drawSubMesh(const CMesh* mesh, const std::string& name) {
    if (!mesh) return;
    uint32_t first = 0, count = 0;
    mesh->getSubMeshData(name, first, count);   // logs "existiert nicht" and returns 0/0 when missing
    mesh->draw(first, count);
}

}  // namespace py
