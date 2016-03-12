//
// Created by simon on 2016-02-06.
//

#ifndef SUPER_BUBBA_AWESOME_SPACE_LAYOUT_H
#define SUPER_BUBBA_AWESOME_SPACE_LAYOUT_H

#include <vector>
#include <map>
#include <Dimension.h>

using namespace std;

class Texture;
class HUDGraphic;
class IHudDrawable;

class Layout {

public:

    virtual void addChild(Layout* child);
    virtual map<string,IHudDrawable*> getGLSquares(float layoutXPos,float layoutYPos, float layoutWidth,
                                           float layoutHeight);

    /**
     * \warn Never allowed to return a wrapping dimension
     */
    virtual Dimension getWidth() = 0;
    /**
     * \warn Never allowed to return a wrapping dimension
     */
    virtual Dimension getHeight() = 0;

    virtual ~Layout(){}
    virtual void getGLSquares(float layoutXPos,float layoutYPos, float layoutWidth,
                              float layoutHeight, map<string,IHudDrawable*>* list) = 0;

    virtual void setBackground(HUDGraphic* graphic);

    virtual Layout* setId(string id);

    virtual Layout* findById(string id);

protected:
    vector<Layout*> children;
    HUDGraphic* graphic = nullptr;
    string id = "";

    string getNextRandId();

};

#endif //SUPER_BUBBA_AWESOME_SPACE_LAYOUT_H