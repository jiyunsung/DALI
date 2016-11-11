/*
    This file is part of Dirt, the Dartmouth introductory ray tracer, used in
    Dartmouth's COSC 77/177 Computer Graphics course.

    Copyright (c) 2016 by Wojciech Jarosz

    Dirt is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Dirt is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "vector.h"
#include "color.h"
#include "parser.h"

//! A class used to represent surface material properties.
/*!
    We currently only support a Blinn-Phong material. This class could be made
    into a virtual base class to support other types of materials.
 */
class Material
{
public:
    Material(const json & j = json())
    {
        Parser::get(j, kd, "kd");
        Parser::get(j, ks, "ks");
        Parser::get(j, kr, "kr");
        Parser::get(j, kt, "kt");
        Parser::get(j, n, "n");
        Parser::get(j, ior, "ior");
    }

    Color3f shade(const Ray3f & ray, const class Intersection3f & its,
                  const class Scene & scene) const;

public:
    Color3f kd = Color3f::Ones();           //!< diffuse coefficient
    Color3f ks = Color3f::Zero();           //!< specular coefficient
    float n = 10;                           //!< specular exponent
    float ior = 1.5;                        //!< index of refraction
    Color3f kr = Color3f::Zero();           //!< reflection coefficient
    Color3f kt = Color3f::Zero();           //!< transmission coefficient

};
