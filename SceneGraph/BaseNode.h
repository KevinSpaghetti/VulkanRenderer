//
// Created by Kevin on 21/12/2020.
//

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <list>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Visitor.h"
#include "Material.h"
#include "Geometry.h"

class BaseNode {
protected:

    glm::mat4 mModel;
    glm::mat4 mModelInverse;

    glm::mat4 mScale;
    glm::mat4 mScaleInverse;
    glm::mat4 mTranslate;
    glm::mat4 mTranslateInverse;
    glm::mat4 mRotate;
    glm::mat4 mRotateInverse;

private:
    std::string mName;

    const BaseNode* mParent = nullptr;
    std::list<std::shared_ptr<BaseNode>> mChildren;

    bool mToUpdate = true;

    //Scene graph changes events
    virtual void nodeAdded(const BaseNode* to){
        mParent = to;
        transformUpdated();
    }
    virtual void nodeRemoved(const BaseNode* parent){
        mParent = nullptr;
    }

protected:
    void transformUpdated(){
        if(mParent == nullptr) {
            mModel = (mRotate * mScale * mTranslate);
            mModelInverse = (mTranslateInverse * mScaleInverse * mRotateInverse);
        }else{
            mModel = mParent->mModel * (mRotate * mScale * mTranslate);
            mModelInverse = (mTranslateInverse * mScaleInverse * mRotateInverse) * mParent->mModelInverse;
        }

        for(auto child : mChildren){
            child->transformUpdated();
        }
    }


public:

    BaseNode(const std::string name){
        mName = name;
        mModel = mModelInverse = glm::mat4(1.0f);
        mScale = mScaleInverse = glm::mat4(1.0f);
        mTranslate = mTranslateInverse = glm::mat4(1.0f);
        mRotate = mRotateInverse = glm::mat4(1.0f);
    }

    void addScale(const glm::vec3 vec) {
        mScale = glm::scale(mScale, vec);
        mScaleInverse = glm::scale(mScaleInverse, -vec);
        transformUpdated();
    }
    void addTranslation(const glm::vec3 vec) {
        mTranslate = glm::translate(mTranslate, vec);
        mTranslateInverse = glm::translate(mTranslateInverse, -vec);
        transformUpdated();
    }
    void addRotation(const glm::vec3 vec, float radians) {
        mRotate = glm::rotate(mRotate, radians, vec);
        mRotateInverse = glm::rotate(mRotateInverse, radians, vec);
        transformUpdated();
    }

    void setScale(const glm::vec3 vec) {
        mScale = glm::scale(glm::mat4(1.0f), vec);
        mScaleInverse = glm::scale(glm::mat4(1.0f), -vec);
        transformUpdated();
    }
    void setTranslation(const glm::vec3 vec) {
        mTranslate = glm::translate(glm::mat4(1.0f), vec);
        mTranslateInverse = glm::translate(glm::mat4(1.0f), -vec);
        transformUpdated();
    }
    void setRotation(const glm::vec3 vec, float radians) {
        mRotate = glm::rotate(glm::mat4(1.0f), radians, vec);
        mRotateInverse = glm::rotate(glm::mat4(1.0f), radians, vec);
        transformUpdated();
    }

    void setScale(const glm::mat4 transform) {
        mScale = transform;
        mScaleInverse = glm::inverse(transform);
        transformUpdated();
    }
    void setTranslation(const glm::mat4 transform) {
        mTranslate = transform;
        mTranslateInverse = glm::inverse(transform);
        transformUpdated();
    }
    void setRotation(const glm::mat4 transform) {
        mRotate = transform;
        mRotateInverse = glm::inverse(transform);
        transformUpdated();
    }

    bool operator ==(const BaseNode& rhs) const {
        if (this == &rhs){
            return true;
        }
        if (mName == rhs.mName){
            return true;
        }
        return false;
    }

    std::string name() const {
        return mName;
    }

    const std::list<std::shared_ptr<BaseNode>> children() const {
        return mChildren;
    }

    void addChild(std::shared_ptr<BaseNode> node){
        mChildren.push_back(node);
        node->nodeAdded(this);
    }
    void removeChild(std::shared_ptr<BaseNode> node){
        mChildren.remove(node);
        node->nodeRemoved(this);
    }

    bool toUpdate() {
        return mToUpdate;
    }
    void setToUpdate() {
        mToUpdate = true;
        for(auto child : mChildren) child->setToUpdate();
    }
    void updated() {
        mToUpdate = false;
    }

    glm::mat4 modelMatrix() const {
        return mModel;
    }
    virtual void accept(Visitor* v) {
        v->visit(this);
    };

};

class ObjectNode : public BaseNode {
private:

    Geometry mObjectGeometry;
    Material mObjectMaterial;

    UniformSet mObjectSet;

public:
    ObjectNode(const std::string name,
            const Geometry geometry,
            const Material material) :
            BaseNode(name),
            mObjectGeometry(geometry),
            mObjectMaterial(material){
        mObjectSet.slot = 0;
        mObjectSet.uniforms[0] = Uniform{TYPE_BUFFER, {4, 4, 0},
                                        sizeof(glm::mat4),1,
                                        std::make_shared<glm::mat4>(1.0)};
        mObjectSet.uniforms[1] = Uniform{TYPE_BUFFER, {4, 4, 0},
                                        sizeof(glm::mat4),1,
                                        std::make_shared<glm::mat4>(1.0)};
        mObjectSet.uniforms[2] = Uniform{TYPE_BUFFER, {4, 4, 0},
                                        sizeof(glm::mat4),1,
                                        std::make_shared<glm::mat4>(1.0)};
        mObjectSet.uniforms[3] = Uniform{TYPE_BUFFER, {3, 1, 0},
                                        sizeof(glm::vec3),1,
                                        std::make_shared<glm::vec3>(1.0)};
    }

    static std::map<std::string, UniformSet> getObjectSetArchetype(){
        UniformSet objectSetArchetype;
        objectSetArchetype.slot = 0;
        objectSetArchetype.uniforms[0] = Uniform{TYPE_BUFFER, {4, 4, 0},
                                         sizeof(glm::mat4),1,
                                         std::make_shared<glm::mat4>(1.0)};
        objectSetArchetype.uniforms[1] = Uniform{TYPE_BUFFER, {4, 4, 0},
                                         sizeof(glm::mat4),1,
                                         std::make_shared<glm::mat4>(1.0)};
        objectSetArchetype.uniforms[2] = Uniform{TYPE_BUFFER, {4, 4, 0},
                                         sizeof(glm::mat4),1,
                                         std::make_shared<glm::mat4>(1.0)};
        objectSetArchetype.uniforms[3] = Uniform{TYPE_BUFFER, {3, 1, 0},
                                         sizeof(glm::vec3),1,
                                         std::make_shared<glm::vec3>(1.0)};
        return {{"object", objectSetArchetype},
                {"material", Material::getMaterialSetArchetype()}};
    }


    Geometry getGeometry() const {
        return mObjectGeometry;
    }

    Material getMaterial() const {
        return mObjectMaterial;
    }

    std::map<std::string, UniformSet> getUniformSets() const {
        return {{"object", mObjectSet},
                {"material", mObjectMaterial.uniforms()}};
    }

    void accept(Visitor* v) override {
        v->visit(this);
    }

private:

};
class CameraNode : public BaseNode {
private:
    bool active;
    glm::mat4 mProjection;

public:
    CameraNode(const std::string& name,
            bool active,
            float fov,
            float width,
            float height,
            float near,
            float far) : BaseNode(name), active(active) {
        mProjection = glm::perspective(fov, width / height, near, far);
        mProjection[1][1] *= -1;
    }

    bool isActive() const {
        return active;
    }

    glm::mat4 getViewMatrix() const {
        return mModelInverse;
    }
    glm::mat4 getProjectionMatrix() const {
        return mProjection;
    }

    void accept(Visitor* v) override {
        v->visit(this);
    }

};
class LightNode : public BaseNode {
private:
    float strength;
    glm::vec3 color;

    glm::mat4 mProjection;

public:

    LightNode(const std::string& name, float strength, glm::vec3 color, float side) : BaseNode(name), strength(strength), color(color) {
        mProjection = glm::perspective(45.0, 1.0, 0.1, 1000.0);
        mProjection[1][1] *= -1;
    }


    glm::mat4 getViewMatrix() const {
        return mModelInverse;
    }
    glm::mat4 getProjectionMatrix() const {
        return mProjection;
    }

    void accept(Visitor* v) override {
        v->visit(this);
    }
};