#ifndef DATA_STORE_H_
#define DATA_STORE_H_

#include "common_include.h"

class DataStore
{
private:
    std::unordered_map<std::string, std::string>    m_map;
    mutable std::shared_mutex                       m_mutex;

public:
    bool set(std::string key, std::string value);

    bool del(std::string key);

    std::tuple<bool, std::string> get(std::string key);

    bool set(const char* key, const char* value)
    {
        return set(std::string(key), std::string(value));
    }

    bool del(const char* key)
    {
        return del(std::string(key));
    }

    std::tuple<bool, std::string> get(const char* key)
    {
        return get(std::string(key));
    }
};

#endif /* #ifndef DATA_STORE_H_ */