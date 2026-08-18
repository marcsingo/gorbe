#ifndef GORBE_SCENE_TIME_HPP
#define GORBE_SCENE_TIME_HPP

// A képletekben használható `t` — az eltelt idő másodpercben.
//
// Beépített név: NEM lehet paraméterként felvenni (a foglalt nevek közt van), viszont
// bármelyik képletben hivatkozható. A kifejezésfa a lenti float CÍMÉT tárolja, ezért
// az érték frissítése önmagában hat: nem kell újraparseolni és újraderiválni.
//
// Egyetlen, PROGRAM-szintű óra van (nem fülönkénti): a `t` mindig "a mostani idő".
// Ebből következik, hogy egy háttérben álló fül alakzata a visszaváltáskor a
// megváltozott t-hez ugrik — ez a "jelenlegi idő" jelentésből egyenesen adódik.
namespace SceneTime {

    inline float value = 0.0f;

    inline float const* ptr() { return &value; }

}

#endif //GORBE_SCENE_TIME_HPP
