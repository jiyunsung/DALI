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

#include "material.h"
#include "scene.h"
#include "light.h"

Color3f Material::shade(const Ray3f & ray, const Intersection3f & its,
                        const Scene & scene) const
{
	Color3f result = Color3f(0, 0, 0);

	for (int i = 0; i < scene.getLights().size(); i++) {

		Transform transform = Transform(scene.getLights()[i]->xform);
		Point3f lightposition = transform * Point3f(0, 0, 0);
		Vector3f lg = lightposition - its.p;
		auto shadRay = Ray3f(its.p, lg, Epsilon, lg.norm() - Epsilon);
		
		Intersection3f shadIts;

		if (!scene.intersect(shadRay, shadIts)) {
			Vector3f l = lg.normalized();
			// compute shading

			Color3f diffuselight = its.mat->kd * 1/lg.dot(lg) *scene.getLights()[i]->intensity * std::max((float)0, its.sn.dot(l));
			result = result + diffuselight;

			Vector3f v = ray.d.normalized();
			Vector3f h = (-v + l).normalized();
			Color3f BlinnPhongLight = its.mat->ks * 1 / lg.dot(lg) * scene.getLights()[i]->intensity * powf(max((float)0, its.sn.dot(h)), its.mat->n);
			result = result + BlinnPhongLight;
		}
	}

	Vector3f v = ray.d.normalized();

	if (its.mat->kr.x() != 0 || its.mat->kr.y() != 0 || its.mat->kr.z() != 0) {
		Vector3f r = 2 * its.sn * (its.sn.dot(-v)) + v;
		Color3f reflectedLight = its.mat->kr * scene.radiance(Ray3f(its.p, r));

		result = result + reflectedLight;
	}

	return result;

	// TODO: Assignment 1. In pseudo code:
    // accumulate color starting with ambient
    // foreach light
        // compute light response
        // compute light direction
        // compute the material response (brdf*cos)
        // check for shadows and accumulate if needed
    // if the material has reflections
        // create the reflection ray
        // accumulate the reflected light (recursive call) scaled by the material reflection
    // return the accumulated color (for now zero)
}
