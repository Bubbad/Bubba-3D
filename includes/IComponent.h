/*
 * This file is part of Bubba-3D.
 *
 * Bubba-3D is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Bubba-3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Bubba-3D. If not, see http://www.gnu.org/licenses/.
 */

#ifndef BUBBA_3D_COMPONENT_H
#define BUBBA_3D_COMPONENT_H

#include <memory>

class GameObject;

class IComponent {
public:
    virtual void update(float dt) = 0;
    void bind(std::weak_ptr<GameObject> owner) { this->owner = owner; };

    virtual void beforeCollision(std::shared_ptr<GameObject> collider) {};
    virtual void duringCollision(std::shared_ptr<GameObject> collider) {};
    virtual void afterCollision(std::shared_ptr<GameObject> collider) {};
    virtual void onDeath(){ };
protected:
    std::weak_ptr<GameObject> owner;
};

#endif //BUBBA_3D_COMPONENT_H

