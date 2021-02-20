//
// Created by Kevin on 23/12/2020.
//

#pragma once

#include "Visitor.h"

class CollectObjectsVisitor : public Visitor {
private:
    std::vector<ObjectNode*> objects;

public:
    CollectObjectsVisitor() = default;

    void visit(ObjectNode* node) override {
        objects.push_back(node);
    }

    void visit(BaseNode* node){};
    void visit(LightNode* node){};
    void visit(CameraNode* node){};

    std::vector<ObjectNode*> collected() const {
        return objects;
    }
};


class FindActiveCameraVisitor : public Visitor {
private:
    const CameraNode* activeCamera = nullptr;

public:
    FindActiveCameraVisitor() = default;

    void visit(ObjectNode* node) {};
    void visit(BaseNode* node){};
    void visit(LightNode* node){};
    void visit(CameraNode* node){
        if(node->isActive()){
            activeCamera = node;
        }
    };

    const CameraNode* collected() const {
        return activeCamera;
    }
};