// CParticleManager.cpp - see CParticleManager.h.
#include "game/CParticleManager.h"

#include <algorithm>

#include "core/CUtils.h"
#include "game/CParticleSystemExplosion.h"
#include "game/CParticleSystemInfo.h"
#include "game/CParticleSystemSelect.h"
#include "game/CParticleSystemShotConstant.h"
#include "game/CParticleSystemShotDirected.h"
#include "game/CParticleSystemSmoke.h"
#include "game/CParticleSystemThruster.h"

namespace py {

// ------------------------------------------------------------------ CParticleSystemTable

namespace {
// libstdc++'s __prime_list as linked into libProjectY.so (0x4d1d64); the
// policy searches entries 5..255 (ELF 0x44dee4).
const uint32_t kPrimes[] = {
    2u, 3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u, 41u, 43u, 47u, 53u, 59u, 61u, 67u, 71u, 73u, 79u, 83u,
    89u, 97u, 103u, 109u, 113u, 127u, 137u, 139u, 149u, 157u, 167u, 179u, 193u, 199u, 211u, 227u, 241u, 257u, 277u,
    293u, 313u, 337u, 359u, 383u, 409u, 439u, 467u, 503u, 541u, 577u, 619u, 661u, 709u, 761u, 823u, 887u, 953u,
    1031u, 1109u, 1193u, 1289u, 1381u, 1493u, 1613u, 1741u, 1879u, 2029u, 2179u, 2357u, 2549u, 2753u, 2971u, 3209u,
    3469u, 3739u, 4027u, 4349u, 4703u, 5087u, 5503u, 5953u, 6427u, 6949u, 7517u, 8123u, 8783u, 9497u, 10273u,
    11113u, 12011u, 12983u, 14033u, 15173u, 16411u, 17749u, 19183u, 20753u, 22447u, 24281u, 26267u, 28411u,
    30727u, 33223u, 35933u, 38873u, 42043u, 45481u, 49201u, 53201u, 57557u, 62233u, 67307u, 72817u, 78779u,
    85229u, 92203u, 99733u, 107897u, 116731u, 126271u, 136607u, 147793u, 159871u, 172933u, 187091u, 202409u,
    218971u, 236897u, 256279u, 277261u, 299951u, 324503u, 351061u, 379787u, 410857u, 444487u, 480881u, 520241u,
    562841u, 608903u, 658753u, 712697u, 771049u, 834181u, 902483u, 976369u, 1056323u, 1142821u, 1236397u,
    1337629u, 1447153u, 1565659u, 1693859u, 1832561u, 1982627u, 2144977u, 2320627u, 2510653u, 2716249u, 2938679u,
    3179303u, 3439651u, 3721303u, 4026031u, 4355707u, 4712381u, 5098259u, 5515729u, 5967347u, 6456007u, 6984629u,
    7556579u, 8175383u, 8844859u, 9569143u, 10352717u, 11200489u, 12117689u, 13109983u, 14183539u, 15345007u,
    16601593u, 17961079u, 19431899u, 21023161u, 22744717u, 24607243u, 26622317u, 28802401u, 31160981u,
    33712729u, 36473443u, 39460231u, 42691603u, 46187573u, 49969847u, 54061849u, 58488943u, 63278561u,
    68460391u, 74066549u, 80131819u, 86693767u, 93793069u, 101473717u, 109783337u, 118773397u, 128499677u,
    139022417u, 150406843u, 162723577u, 176048909u, 190465427u, 206062531u, 222936881u, 241193053u, 260944219u,
    282312799u, 305431229u, 330442829u, 357502601u, 386778277u, 418451333u, 452718089u, 489790921u, 529899637u,
    573292817u, 620239453u, 671030513u, 725980837u, 785430967u, 849749479u, 919334987u, 994618837u, 1076067617u,
    1164186217u, 1259520799u, 1362662261u, 1474249943u, 1594975441u, 1725587117u, 1866894511u, 2019773507u,
    2185171673u, 2364114217u, 2557710269u, 2767159799u, 2993761039u, 3238918481u, 3504151727u, 3791104843u,
    4101556399u, 4294967291u, 4294967291u};
const uint8_t kFastBuckets[12] = {2, 2, 2, 3, 5, 5, 7, 7, 11, 11, 11, 11};
}  // namespace

// _Prime_rehash_policy::_M_next_bkt. With the max load factor 1.0 the
// policy's double arithmetic (ceil(n * 1.0)) is exact, so integers suffice.
size_t CParticleSystemTable::nextBucketCount(size_t n, size_t* nextResize) {
    size_t r;
    if (n <= 11) {
        r = kFastBuckets[n];
    } else {
        const uint32_t* first = kPrimes + 5;
        const uint32_t* last = first + 251;
        const uint32_t* p = std::lower_bound(first, last, static_cast<uint32_t>(n));
        r = *p;
    }
    *nextResize = r;
    return r;
}

CParticleSystemTable::CParticleSystemTable() { mBuckets.assign(nextBucketCount(10, &mNextResize), nullptr); }

CParticleSystemTable::~CParticleSystemTable() { clear(); }

// _M_rehash_aux (unique keys)
void CParticleSystemTable::rehash(size_t n) {
    std::vector<Node*> nb(n, nullptr);
    Node* p = mBeforeBegin.next;
    mBeforeBegin.next = nullptr;
    size_t bbeginBkt = 0;
    while (p) {
        Node* next = p->next;
        const size_t bkt = p->key % n;
        if (!nb[bkt]) {
            p->next = mBeforeBegin.next;
            mBeforeBegin.next = p;
            nb[bkt] = &mBeforeBegin;
            if (p->next) nb[bbeginBkt] = p;
            bbeginBkt = bkt;
        } else {
            p->next = nb[bkt]->next;
            nb[bkt]->next = p;
        }
        p = next;
    }
    mBuckets.swap(nb);
}

// _M_emplace -> _M_insert_unique_node
void CParticleSystemTable::insert(unsigned key, CParticleSystem* value) {
    if (find(key)) {
        delete value;
        return;
    }
    Node* node = new Node;
    node->key = key;
    node->value = value;
    // _M_need_rehash(bucket_count, element_count, 1)
    const size_t want = mElementCount + 1;
    if (want >= mNextResize) {
        if (want >= mBuckets.size()) {
            const size_t n = std::max(want + 1, mBuckets.size() * 2);   // floor(min_bkts) + 1, n_bkt * growth
            rehash(nextBucketCount(n, &mNextResize));
        } else {
            mNextResize = mBuckets.size();   // floor(n_bkt * 1.0)
        }
    }
    const size_t bkt = bucketOf(key);
    // _M_insert_bucket_begin
    if (mBuckets[bkt]) {
        node->next = mBuckets[bkt]->next;
        mBuckets[bkt]->next = node;
    } else {
        node->next = mBeforeBegin.next;
        mBeforeBegin.next = node;
        if (node->next) mBuckets[bucketOf(node->next->key)] = node;
        mBuckets[bkt] = &mBeforeBegin;
    }
    ++mElementCount;
}

CParticleSystem* CParticleSystemTable::find(unsigned key) const {
    const size_t bkt = bucketOf(key);
    Node* prev = mBuckets[bkt];
    if (!prev) return nullptr;
    for (Node* p = prev->next; p; p = p->next) {
        if (p->key == key) return p->value;
        if (p->next && bucketOf(p->next->key) != bkt) break;
    }
    return nullptr;
}

// _M_erase(bkt, prev, n)
CParticleSystemTable::Node* CParticleSystemTable::erase(Node* n) {
    const size_t bkt = bucketOf(n->key);
    Node* prev = mBuckets[bkt];
    while (prev->next != n) prev = prev->next;
    Node* next = n->next;
    if (prev == mBuckets[bkt]) {
        // _M_remove_bucket_begin
        const size_t nextBkt = next ? bucketOf(next->key) : 0;
        if (!next || nextBkt != bkt) {
            if (next) mBuckets[nextBkt] = mBuckets[bkt];
            if (&mBeforeBegin == mBuckets[bkt]) mBeforeBegin.next = next;
            mBuckets[bkt] = nullptr;
        }
    } else if (next) {
        const size_t nextBkt = bucketOf(next->key);
        if (nextBkt != bkt) mBuckets[nextBkt] = prev;
    }
    prev->next = next;
    delete n;
    --mElementCount;
    return next;
}

void CParticleSystemTable::clear() {
    for (Node* p = mBeforeBegin.next; p;) {
        Node* next = p->next;
        delete p->value;
        delete p;
        p = next;
    }
    std::fill(mBuckets.begin(), mBuckets.end(), nullptr);
    mElementCount = 0;
    mBeforeBegin.next = nullptr;
}

// ------------------------------------------------------------------ CParticleManager

unsigned CParticleManager::mNextParticleSystemID = 1;

CParticleManager::CParticleManager() = default;
CParticleManager::~CParticleManager() = default;

// Ghidra 0x36ca44
CParticleSystem* CParticleManager::createParticleSystem(int type, const CVector3& position, int subtype,
                                                        bool ignoreFOW, CPlayer* player, bool showInMenu) {
    CParticleSystem* ps = nullptr;
    switch (type) {
    case PS_SMOKE: ps = new CParticleSystemSmoke(subtype); break;
    case PS_SELECT: ps = new CParticleSystemSelect(subtype); break;
    case PS_EXPLOSION: ps = new CParticleSystemExplosion(subtype); break;
    case PS_THRUSTER: ps = new CParticleSystemThruster(subtype); break;
    case PS_SHOTDIRECTED: ps = new CParticleSystemShotDirected(subtype); break;
    case PS_SHOTCONSTANT: ps = new CParticleSystemShotConstant(subtype); break;
    case PS_INFO: ps = new CParticleSystemInfo(subtype); break;
    default:
        CUtils::log("ERROR: CParticleManager::createParticleSystem unknown type %i", type);
        return nullptr;
    }
    ps->init(position, player);
    ps->mIgnoreFOW = ignoreFOW;
    ps->mShowInMenu = showInMenu;
    ps->mID = mNextParticleSystemID;
    const unsigned id = mNextParticleSystemID++;
    mSystems.insert(id, ps);
    return ps;
}

// Ghidra 0x36cbc8
CParticleSystem* CParticleManager::createParticleSystem(int type, const CVector2& position, Real z, int subtype,
                                                        bool ignoreFOW, CPlayer* player, bool showInMenu) {
    const CVector3 p(position.x, position.y, z);
    return createParticleSystem(type, p, subtype, ignoreFOW, player, showInMenu);
}

// Ghidra 0x36c684
CParticleSystem* CParticleManager::getParticleSystem(unsigned id) const { return mSystems.find(id); }

// Ghidra 0x36c6f8
void CParticleManager::disableParticleSystem(unsigned& id) {
    if (id == 0) return;
    if (CParticleSystem* ps = getParticleSystem(id)) ps->mDeleteRequested = true;
    id = 0;
}

void CParticleManager::attachParticleSystem(unsigned& id, const CVector3& position) {
    if (id == 0) return;
    if (CParticleSystem* ps = getParticleSystem(id))
        ps->mPosition = position;
    else
        id = 0;
}

// Ghidra 0x36c524
void CParticleManager::timeUpdate(Real dt) {
    for (CParticleSystemTable::Node* n = mSystems.begin(); n;) {
        CParticleSystem* ps = n->value;
        if (!ps->mDeleteRequested || ps->numParticles() != 0) {
            ps->timeUpdate(dt);
            n = n->next;
        } else {
            n = mSystems.erase(n);
            delete ps;
        }
    }
}

// Ghidra 0x36c7f8
void CParticleManager::cleanUp() { mSystems.clear(); }

}  // namespace py
