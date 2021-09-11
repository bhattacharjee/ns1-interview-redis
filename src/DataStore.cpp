#include "DataStore.h"

bool DataStore::set(std::string key, std::string value)
{
    std::unique_lock lock(m_mutex);
    try
    {
        m_map[key] = value;
    }
    catch(...)
    {
        return false;
    }

    return true;
}

bool DataStore::del(std::string key)
{
    std::unique_lock lock(m_mutex);
    try
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return false;
        m_map.erase(it);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::tuple<bool, std::string> DataStore::get(std::string key)
{
    std::shared_lock lock(m_mutex);
    try
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return std::make_tuple(false, std::string(""));
        return std::make_tuple(true, m_map[key]);
    }
    catch (...)
    {
        return std::make_tuple(false, std::string(""));
    }
}