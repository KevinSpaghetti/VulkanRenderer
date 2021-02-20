//
// Created by Kevin on 22/01/2021.
//

#pragma once

class Window {
private:
    std::string m_title;
    glm::ivec2 m_size;

    GLFWwindow *m_window;

public:
    Window(std::string title,
    int width,
    int height) :
    m_title(title),
    m_size({width, height}){
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(
                m_size.x,
                m_size.y,
                m_title.c_str(),
                nullptr, nullptr);


    }

    GLFWwindow* getWindowHandle() const {
        return m_window;
    }

    glm::ivec2 getWindowSize() const {
        return m_size;
    }

    void pollEvents(){
        glfwPollEvents();
    }

    bool windowShouldClose() const {
        return glfwWindowShouldClose(m_window);
    }

    ~Window(){
        if (m_window != nullptr) {
            glfwDestroyWindow(m_window);
        }
        glfwTerminate();
    }

};