//
// Created by Kevin on 23/12/2020.
//

#pragma once

class BaseNode;
class ObjectNode;
class LightNode;
class CameraNode;

class Visitor {
public:
    virtual void visit(BaseNode* node) = 0;
    virtual void visit(ObjectNode* node) = 0;
    virtual void visit(LightNode* node) = 0;
    virtual void visit(CameraNode* node) = 0;
};