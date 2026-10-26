// CGamePreview.cpp - a stand-in renderer for game objects until phase 4.
//
// NOT a port of CGameObject::draw: it draws each building / unit with its
// main submesh of gameobjects.3DS.mesh (names from the classes' init
// functions), at its position on the z = 0 plane, rotated about z, in its
// owner's colour - enough to see what a map script created. Drivers sitting
// in vehicles and beacons are not drawn. Phase 4 replaces this with the real
// draw code (multi-part units, z offsets, animations, FOW).
#include <cmath>

#include "game/CBuilding.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
const char* buildingMesh(int type) {
    static const char* k[] = {nullptr,     "Flagpost1",  "Production", "HQ1",    "SpecOp1",
                              "Repair",    "Productio1", "Productio0", "trainingce", "advfactory",
                              "sclab1",    "bccbase1",   "shldbld",    "infostore1", "ulterminal",
                              "Refinery"};
    return type >= 1 && type <= 15 ? k[type] : nullptr;
}
const char* unitMesh(int type) {
    static const char* k[] = {nullptr,     "Robobrain", "Spider1",  "Scorpion1", "Turret",  "Tank1",
                              "Bombbrain1", "Crate1",   "URepair1", "SoldierBra", "HeavyT1", nullptr,
                              "spotter1",  "EMPT2B1",   "minelayer1", "landmine1", "emptank1", "commander",
                              "amplifier", "sniper1",   "cloaker",  "hacker1",   "tbrain1", "obrain1",
                              "Harvester"};
    return type >= 1 && type <= 24 ? k[type] : nullptr;
}
}  // namespace

void CGame::drawPreview(const CCameraLevel& cam) const {
    if (!mLevel || cam.isStrategyView()) return;
    CMesh* mesh = CMeshManager::get().getMesh("gameobjects.3DS.mesh", true, false);
    CTexture* tex = CTextureManager::get().getTexture("gameobjects.png", 1, 2, 2, true);
    CGLSLProgram* sh = CGLSLProgramManager::get().getGLSLProgram("diffuseplayerlight.gfx");
    if (!mesh || !tex || !sh) return;
    sh->setupUniformTextureUnit("DIFFUSE", 0);
    mesh->bindBuffer();
    sh->startDraw();
    tex->bind(0x84C0);
    sh->setUniformLightColor(mLevel->lightColor());
    for (unsigned id : mObjectList) {
        const CGameObject* o = getGameObject(id);
        if (!o) continue;
        const char* name = nullptr;
        if (o->mCategory == CGameObject::CATEGORY_BUILDING) {
            name = buildingMesh(o->mType);
        } else {
            const auto* u = static_cast<const CUnit*>(o);
            if (u->mCommandedUnit != 0) continue;   // a driver inside a vehicle
            name = unitMesh(o->mType);
        }
        uint32_t first = 0, count = 0;
        if (!name || !mesh->getSubMeshData(name, first, count)) continue;
        const float h = o->mYaw * 0.5f;
        const Quat q(std::cos(h), 0.0f, 0.0f, std::sin(h));
        const Mat4 model = Mat4::transform({o->mPosition.x, o->mPosition.y, 0.0f}, q, {1, 1, 1});
        sh->setUniformMVPMatrix(cam.viewProj() * model);
        sh->setUniformLightDir(q.inverse().rotate(mLevel->lightDirection()));
        sh->setUniformPlayerColor(o->mOwner ? o->mOwner->mColor : Vec3(0.7f, 0.7f, 0.7f));
        mesh->draw(first, count);
    }
    sh->endDraw();
}

}  // namespace py
