#include "../../thirdparty/json.hpp"
#include <iostream>
#include <string>

using namespace std;
using json = nlohmann::json;

int main()
{
    json jsSend;
    jsSend["id"] = 2;
    jsSend["msg"] = "xixi";
    jsSend["msgid"] = 6;
    jsSend["name"] = "cm";
    jsSend["time"] = "2026-05-26 23:39:58";
    jsSend["toid"] = 1;

    string str = jsSend.dump();
    json js = json::parse(str);

    cout << str.size() << endl;
    cout << js << endl;
    
    return 0;
}