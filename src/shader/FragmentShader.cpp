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
//
// Created by johan on 2016-03-12.
//

#include "FragmentShader.h"

#define FRAGMENT_SHADER_NAME_STRING "Vertex_Shader"

FragmentShader::FragmentShader(std::string shaderName) {
    shaderString = textFileRead(shaderName.c_str(), true);
    compile();
}

void FragmentShader::compile() {
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, shaderString.c_str());
    checkErrors(fragmentShader);
}

void FragmentShader::checkErrors(GLuint &shader) {
    checkCompileErrors(&shader, FRAGMENT_SHADER_NAME_STRING);
}

GLuint FragmentShader::getGLId() {
    return this->fragmentShader;
}

