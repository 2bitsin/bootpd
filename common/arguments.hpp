#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include "lexical_cast.hpp"

struct arguments
{
  arguments(std::vector<std::string_view> _args);
  arguments(int argc, char** argv);

  auto has(std::string_view _key) const -> bool;

  template<typename... _Key>
  auto has(std::string_view _key, _Key... _keys) const -> bool
  {
    return has(_key) || has(_keys...);
  }

  auto value(std::string_view& _value, std::string_view _key) const -> bool;  
  auto value_or(std::string_view _key, std::string_view _default) const -> std::string_view;
  auto values(std::string_view _key) const -> const std::vector<std::string_view>&;

  template <typename _Type>
  auto value(_Type& _value, std::string_view _key) -> bool
  {
    try
    {
      if (m_values.count(_key) > 0)
      {
        _value = lexical_cast<_Type>(m_values.at(_key).front());
        return true;
      }
    }
    catch(const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      return false;
    }    
    return false;
  }
  template<typename _Type>
  auto values(std::string_view _key) -> std::vector<_Type>
  {
    try
    {
      std::vector<_Type> values;
      for (auto&& value : m_values.at(_key))
      {
        values.push_back(lexical_cast<_Type>(value));
      }
      return values;
    }
    catch(const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      return std::vector<_Type>();
    }
  }

  template<typename _Type>
  auto value_or(std::string_view _key, _Type _default) -> _Type
  {
    try
    {			
      if (m_values.count(_key) > 0) {				
        return lexical_cast<_Type>(m_values.at(_key).front());
			}
    }
    catch(const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      return _default;
    }
    return _default;
  }
     
protected:



private:
  std::vector<std::string_view> m_args;
  std::unordered_set<std::string_view> m_flags;
  std::unordered_map<std::string_view, std::vector<std::string_view>> m_values;
};
