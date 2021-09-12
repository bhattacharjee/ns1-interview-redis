#ifndef DATA_STORE_H_
#define DATA_STORE_H_

#include "common_include.h"

/**
 * @brief This class implements a data store. In essence
 * this is a hash table, with synchronization added
 * for thread safety.
 * 
 * The orchestrator maintains an array of several of these
 * for even greater parallelism.
 * 
 */
class DataStore
{
private:
    /**
     * @brief The hash map for key-value pairs
     * keys are strings, and values are serialized
     * RESP objects
     * 
     */
    std::unordered_map<std::string, std::string>    m_map;

    /**
     * @brief the mutex to serialize the hash table
     * 
     */
    mutable std::shared_mutex                       m_mutex;

public:
    /**
     * @brief set a key-value
     * 
     * @param key
     * @param value 
     * @return true success
     * @return false failure
     */
    bool set(const std::string& key, const std::string& value);

    /**
     * @brief delete a key
     * 
     * @param key 
     * @return true success
     * @return false failure
     */
    bool del(const std::string& key);

    /**
     * @brief fetch a value for a key
     * 
     * @param key 
     * @return std::tuple<bool, std::string> 
     * A tuple containing
     * 1. whether the key was found or not
     * 2. The value
     */
    std::tuple<bool, std::string> get(const std::string& key);

    /**
     * @brief Set a key value pair
     * 
     * @param key 
     * @param value 
     * @return true success
     * @return false failure
     */
    bool set(const char* key, const char* value)
    {
        return set(std::string(key), std::string(value));
    }

    /**
     * @brief delete a key
     * 
     * @param key 
     * @return true success
     * @return false failure
     */
    bool del(const char* key)
    {
        return del(std::string(key));
    }

    /**
     * @brief fetch a ey
     * 
     * @param key 
     * @return std::tuple<bool, std::string> 
     * A tuple containing
     * 1. whether the key was found or not
     * 2. The value
     */
    std::tuple<bool, std::string> get(const char* key)
    {
        return get(std::string(key));
    }
};

#endif /* #ifndef DATA_STORE_H_ */