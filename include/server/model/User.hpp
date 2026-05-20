#pragma once

#include <string>

class User
{
public:
    User(int id = -1, std::string name = "", std::string password = "", std::string state = "offline")
        : _id(id), _name(name), _password(password), _state(state)
    {}

    void setId(const int &id) { _id = id; }
    void setName(const std::string &name) { _name = name; }
    void setPwd(const std::string &password) { _password = password; }
    void setState(const std::string &state) { _state = state; }

    int getId() const { return _id; }
    std::string getName() const { return _name; }
    std::string getPwd() { return _password; }
    std::string getState() { return _state; }

private:
    int _id;
    std::string _name;
    std::string _password;
    std::string _state;
};