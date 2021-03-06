#pragma once

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum BEHAVIOUR {
  STRICT, //"-a 0" and "--append=0" are valid. "--append 0", "-a=0" are not valid.
  AS_IS //"-a 0", "-a=0", "--append 0", "--append=0" are all valid behaviour. Useful for Windows-style flags.
};

class no_argv : public std::runtime_error {
public:
  explicit no_argv(const std::string& what_arg) : std::runtime_error(what_arg) {}
  explicit no_argv(const char* what_arg) : std::runtime_error(what_arg) {}
};

class impossible_case_reached : public std::runtime_error {
public:
  explicit impossible_case_reached(const std::string& what_arg) : std::runtime_error(what_arg) {}
  explicit impossible_case_reached(const char* what_arg) : std::runtime_error(what_arg) {}
};

namespace __internal {
  template <typename S>
  S stripQuotes(const S& s) { //strip the quotes out of a string, if there are matching ones
    if ((s.find("\"") == 0 && s.find_last_of("\"") == s.size() - 1) ||
	(s.find("\'") == 0 && s.find_last_of("\'") == s.size() - 1)) {
      return s.substr(1, s.size() - 2);
    }
    return s;
  }
};

/* The implementation of GetOpt. All invalid flags are returned by readArgs
   Template:
   I: The integer type to use
   C: The character type to use
   F: The floating point type to use (either double or float)
   S: The string type to use
*/
template <typename I, typename C, typename F, typename S>
class __impl_GetOpt {
public:
  using integer_t = I;
  using character_t = C;
  using floating_t = F;
  using string_t = S;
  
  __impl_GetOpt()=default;
  __impl_GetOpt(const __impl_GetOpt&)=default;
  __impl_GetOpt(__impl_GetOpt&&)=default;
  virtual ~__impl_GetOpt();

  /**
    Constructs the class with specified parameters.
    @arg argc - The argument count within argv.
    @arg argv - The argument array to process, delimited by spaces.
    @arg isFromSystem - Set to false if the given argc and argv is generated by caller, not by the system. (Prevents the skipping of argv[0], which is the executable path)
  */
  __impl_GetOpt(integer_t argc, character_t** argv, bool isFromSystem = true) {
    initialize(argc, argv, isFromSystem);
  }

  /* Functions */
  /**
     Initialize the class with specified parameters.
     @arg argc - The argument count within argv
     @arg argv - The argument array to process, delimited by spaces
     @arg isFromSystem - Set to false if the given argc and argv is generated by caller, not by the system. (Prevents the skipping of argv[0], which is the executable path)
  */
  void initialize(const integer_t& argc, character_t** argv, const bool& isFromSystem = true);

  /**
     flag[x] implements a flag with "flagName".
     @arg flagName - Names of the flag, separated by commas (,). Example: "-a,--append,--add-behind"
     @arg default_value (optional) - The default value of the flags
     Returns the pointer to the variable created by the function. Garbage management is done by the caller.
     May throw errors related to: Memory Allocation (std::bad_alloc), Stream Management (all istream exceptions) and Maps.
  */
  bool* flagBoolean(const string_t& flagName, const bool& default_value = false);
  floating_t* flagFloating(const string_t& flagName, const floating_t& default_value = 0);
  integer_t* flagInt(const string_t& flagName, const integer_t& default_value = 0);
  string_t* flagString(const string_t& flagName, const string_t& default_value = "");

  /**
     Read all arguments from the command line args.
     @arg behaviour (optional) - The behaviour of the function. By default, this is set to STRICT.
     Returns the vector of the invalid flags found.
     May throw errors related to: Stream Management (all istream exceptions), String Manipulation, and Maps.
     Will also throw a custom error (no_argv) if object was not initialized.
  */
  std::vector<string_t> readArgs(BEHAVIOUR behaviour = STRICT);

  /**
     Returns the options acquired from the latest readArgs.
  */
  std::vector<string_t> getOptions() { return options; }
protected:
  //Internal implementation of STRICT and ASIS readArgs modes
  std::vector<string_t> __internal_readArgs_STRICT();
  std::vector<string_t> __internal_readArgs_AS_IS();
private:
  integer_t _argc = 0;
  character_t **_argv = nullptr;
  bool _from_sys = true;

  std::vector<character_t*> allocs = {}; //the allocations done by the class
  std::vector<string_t> options = {}; //the list of options

  enum TYPE { //enum used by the main lookup table to redirect flag searches
    BOOL,
    FLOAT,
    INT,
    STR
  };

  //Lookup tables. As they are std::map objects, lookups are insanely quick
  std::map<std::string, TYPE> main_lookup = {};
  std::map<std::string, bool*> bool_lookup = {};
  std::map<std::string, floating_t*> float_lookup = {};
  std::map<std::string, integer_t*> int_lookup = {};
  std::map<std::string, string_t*> str_lookup = {};
};

template <typename I, typename C, typename F, typename S>
__impl_GetOpt<I, C, F, S>::~__impl_GetOpt<I, C, F, S>() {
  for (auto&& it : allocs) //remove all allocations done by this class, leaving the rest of argv alone
    delete[] it;
}

template <typename I, typename C, typename F, typename S>
void __impl_GetOpt<I, C, F, S>::initialize(const __impl_GetOpt<I, C, F, S>::integer_t& argc, __impl_GetOpt<I, C, F, S>::character_t** argv, const bool& isFromSystem) {
  _argc = argc;
  _argv = argv;
  _from_sys = isFromSystem;
}

#define SETUP_FLAG(TYPE_CPP, TYPE, TYPE_CAPS) /* TYPE_CPP is the actual type in C++ language, TYPE and TYPE_CAPS are the representations that is in the enum 'TYPE'. See definition of flagBoolean() for an example */			\
  std::basic_istringstream<typename string_t::value_type> ss(flagName); /* extracts each flag via a stream */ \
  string_t flag = ""; /* the current flag being processed */ \
  TYPE_CPP *returnVal = new TYPE_CPP(default_value); \
  while (!std::getline(ss, flag, ',').eof()) { /* get flags until end of line */ \
  main_lookup.insert(std::make_pair(flag, TYPE_CAPS)); \
  TYPE##_lookup.insert(std::make_pair(flag, returnVal)); \
  }\
  main_lookup.insert(std::make_pair(flag, TYPE_CAPS)); \
  TYPE##_lookup.insert(std::make_pair(flag, returnVal)); \
  return returnVal

template <typename I, typename C, typename F, typename S>
bool* __impl_GetOpt<I, C, F, S>::flagBoolean(const __impl_GetOpt<I, C, F, S>::string_t& flagName, const bool& default_value) {
  SETUP_FLAG(bool, bool, BOOL);
}

template <typename I, typename C, typename F, typename S>
typename __impl_GetOpt<I, C, F, S>::floating_t* __impl_GetOpt<I, C, F, S>::flagFloating(const __impl_GetOpt<I, C, F, S>::string_t& flagName, const __impl_GetOpt<I, C, F, S>::floating_t& default_value) {
  SETUP_FLAG(floating_t, float, FLOAT);
}

template <typename I, typename C, typename F, typename S>
typename __impl_GetOpt<I, C, F, S>::integer_t* __impl_GetOpt<I, C, F, S>::flagInt(const __impl_GetOpt<I, C, F, S>::string_t& flagName, const __impl_GetOpt<I, C, F, S>::integer_t& default_value) {
  SETUP_FLAG(integer_t, int, INT);
}

template <typename I, typename C, typename F, typename S>
typename __impl_GetOpt<I, C, F, S>::string_t* __impl_GetOpt<I, C, F, S>::flagString(const __impl_GetOpt<I, C, F, S>::string_t& flagName, const __impl_GetOpt<I, C, F, S>::string_t& default_value) {
  SETUP_FLAG(string_t, str, STR);
}

template <typename I, typename C, typename F, typename S>
std::vector<typename __impl_GetOpt<I, C, F, S>::string_t> __impl_GetOpt<I, C, F, S>::readArgs(BEHAVIOUR behaviour) {
  if (_argv == nullptr)
    throw no_argv("__impl_GetOpt<I, C, F, S>::readArgs(BEHAVIOUR) could not find a valid argv.");
  
  switch (behaviour) {
  case STRICT:
    return __internal_readArgs_STRICT();
    break;
  case AS_IS:
    return __internal_readArgs_AS_IS();
    break;
  default: //impossible case
    throw impossible_case_reached("Impossible case reached when trying to interpret behaviour for __impl_GetOpt<I, C, F, S>::readArgs(BEHAVIOUR).");
    break;
  }
}

template <typename I, typename C, typename F, typename S>
std::vector<typename __impl_GetOpt<I, C, F, S>::string_t> __impl_GetOpt<I, C, F, S>::__internal_readArgs_STRICT() {
  //Another check, how safe can you be?
  if (_argv == nullptr)
    throw no_argv("__impl_GetOpt<I, C, F, S>::readArgs(BEHAVIOUR) could not find a valid argv.");

  character_t** _argv_backup = _argv;
  std::vector<string_t> invalid_flags = {};
  integer_t i = _from_sys; //the loop variable (_from_sys is 1 if true, which is exactly where I need to start from)
  while (i < _argc) {
    string_t flag = string_t(_argv[i]);
    const string_t flag_only = flag.substr(0, flag.find('='));
    auto it = main_lookup.find(flag_only); // for the iterator in the lookup table

    if (it == main_lookup.end()) { //flag not in look up table. Either: OPTION, combination flag or unknown flag
      if (main_lookup.begin()->first[0] == flag[0]) //tell if it's a flag; look at first chara in lookup table
	if (main_lookup.find(string_t(flag, 0, 2)) == main_lookup.end()) //checks the first 2 characters in the lookup table. if not found (end), it's probably an invalid flag
	  invalid_flags.push_back(flag);
	else { //the flag might be a combination flag. Split the elements up, and append to the back of argv.
	  string_t s = flag;
	  character_t lastFlag = s.back();
	  s = s.substr(1, s.size() - 2); //ignore the first and last part of the string, which may look like: '-abc', ignoring '-' and 'c'
	  char** newFlags = reinterpret_cast<char**>(std::calloc(_argc + s.size(), sizeof(char**))); //allocate space for the new extra flags
	  std::memcpy(newFlags, _argv, _argc * sizeof(char*)); //copies the pointers to the arguments to the newFlags char** array
	  for (integer_t index = 0; index < s.size(); index++) { //assigns all elements of extraFlags with new flags
	    newFlags[_argc + index] = new character_t[3];
	    allocs.push_back(newFlags[_argc + index]); //adds it to the class allocations
	    std::strcpy(newFlags[_argc + index], string_t("-").append(1, s[index]).c_str());
	  } std::strcpy(newFlags[i], string_t("-").append(1, lastFlag).c_str()); //reassign the flag that we were working on. Reason: '-abc hi' means the flag -c will have the value of hi, so it's better not to move the last flag too.
	  _argc += s.size(); //update argc
	  //NOTE: I did not delete the old _argv, because the user's main() function will still have references to it.
	  _argv = newFlags;
	  continue; //continue. Re evaluate the flag at the current position
	}
      else // it's an option
	options.push_back(flag);
      i++;
      continue;
    } else { //flag in lookup table
      switch (it->second) {
      case BOOL: {
	bool* data = bool_lookup.at(flag_only);
	typename string_t::size_type index = 0;

	if (flag.find("--") == 0) { //if the flag started with two '-', it's a longname flag. Parse with equals.
	  if ((index = flag.find('=')) == std::string::npos) //find an equals. If it doesn't find, set flag to true.
	    *data = true;
	  else //if found, figure out the digit behind the flag. If 0, false. Anything else, true (it's easier).
	    *data = (__internal::stripQuotes(string_t(flag, index + 1)) == "0") ? false : true;
	  i++;
	  continue;
	} else { //the flag started wtih one '-'. it's a shortname flag.
	  if (__internal::stripQuotes(string_t(_argv[index + 1])) == "0" || (*data = ((__internal::stripQuotes(string_t(_argv[index+1])) == "1") ? true : *data))) { //checks and also assigns data to true if detected
	    i += 2;
	    continue;
	  } else { //if there's no second argument, it's also true
	    *data = true;
	    i++;
	    continue;
	  }
	}
	break;
      }

#define HANDLE_NUMERIC_FLAGS(TYPE_CPP, TYPE, CONVERSION_FUNCTION) /* TYPE_CPP is the actual type in C++ language, TYPE is the representation that is in the enum 'TYPE', and CONVERSION_FUNCTION is either std::stod or std::stoi */ \
	  TYPE_CPP* data = TYPE##_lookup.at(flag_only); \
	  typename string_t::size_type index = 0; \
	  if (flag.find("--") == 0) { \
	    if ((index = flag.find('=')) != std::string::npos) /* require an equals */ \
	      *data = CONVERSION_FUNCTION(__internal::stripQuotes(flag.substr(index + 1))); \
	    else invalid_flags.push_back(flag); /* if there is no equals, the flag is invalid */ \
	    i++; \
	    continue; \
	  } else { /* the flag started with one '-'. it's a shortname flag */ \
	    if (i + 1 >= _argc) { /* makes sure that it's possible to obtain the next element */ \
	      invalid_flags.push_back(flag); \
	      i++; \
	      continue; \
	    } else { /* assign data to the next element's equivalent of the numeric type */ \
	      *data = CONVERSION_FUNCTION(__internal::stripQuotes(string_t(_argv[i + 1]))); \
	      i += 2; \
	      continue; \
	    } \
	  }
	
      case FLOAT: {
        HANDLE_NUMERIC_FLAGS(floating_t, float, std::stod)
	break;
      }

      case INT: {
	HANDLE_NUMERIC_FLAGS(integer_t, int, std::stoi)
	break;
      }

      case STR: {
	string_t* data = str_lookup.at(flag_only);
	typename string_t::size_type index = 0;
	if (flag.find("--") == 0) {
	  if ((index = flag.find('=')) != std::string::npos)
	    *data = __internal::stripQuotes(flag.substr(index + 1));
	  else invalid_flags.push_back(flag);
	  i++;
	  continue;
	} else {
	  if (i + 1 >= _argc) {
	    invalid_flags.push_back(flag);
	    i++;
	    continue;
	  } else {
	    *data = __internal::stripQuotes(string_t(_argv[i + 1]));
	    i += 2;
	    continue;
	  }
	}
	break;
      }
      default: {
	throw impossible_case_reached("Impossible case reached in __internal_readArgs_STRICT().");
      }
      }
    }
  }

  // Deletes any new allocations done by this function
  for (auto&& it : allocs)
    delete[] it; //deletes any NEW allocations done by function
  
  if (_argv != _argv_backup) {
    delete[] _argv; //delete current _argv
    _argv = _argv_backup; //restore argv's backup
  }

  allocs.clear();

  return invalid_flags;
}

template <typename I, typename C, typename F, typename S>
std::vector<typename __impl_GetOpt<I, C, F, S>::string_t> __impl_GetOpt<I, C, F, S>::__internal_readArgs_AS_IS() {
  //First part is the same as STRICT, check STRICT for comments
  if (_argv == nullptr)
    throw no_argv("__impl_GetOpt<I, C, F, S>::readArgs(BEHAVIOUR) could not find a valid argv.");

  character_t** _argv_backup = _argv;
  std::vector<string_t> invalid_flags = {};
  integer_t i = _from_sys;

  while (i < _argc) {
    string_t flag = string_t(_argv[i]);
    const string_t flag_only = flag.substr(0, flag.find('='));
    auto it = main_lookup.find(flag_only);

    if (it == main_lookup.end()) {
      if (main_lookup.begin()->first[0] == flag[0])
	if (main_lookup.find(string_t(flag, 0, 2)) == main_lookup.end())
	  invalid_flags.push_back(flag);
	else {
	  string_t s = flag;
	  character_t lastFlag = s.back();
	  s = s.substr(1, s.size() - 2);
	  char** newFlags = reinterpret_cast<char**>(std::calloc(_argc + s.size(), sizeof(char**)));
	  std::memcpy(newFlags, _argv, _argc * sizeof(char*));
	  for (integer_t index = 0; index < s.size(); index++) {
	    newFlags[_argc + index] = new character_t[3];
	    allocs.push_back(newFlags[_argc + index]);
	    std::strcpy(newFlags[_argc + index], string_t("-").append(1, s[index]).c_str());
	  } std::strcpy(newFlags[i], string_t("-").append(1, lastFlag).c_str());
	  _argc += s.size();
	  _argv = newFlags;
	  continue;
    }
      else
	options.push_back(flag);
      i++;
      continue;
    } else {
      switch (it->second) {
      case BOOL: {
	bool* data = bool_lookup.at(flag_only);
	typename string_t::size_type index = 0;

	//First deviation happens here, comments will partially continue now.
	if ((index = flag.find('=')) == std::string::npos) //equal sign does not exist, possibility: no next argument, or has next argument either 0 or 1.
	  if (__internal::stripQuotes(string_t(_argv[index + 1])) == "0" || (*data = ((__internal::stripQuotes(string_t(_argv[index+1])) == "1") ? true : *data))) { //checks and also assigns data to true if detected
	      i += 2;
	      continue;
	    } else { //no second argument
	      *data = true;
	      i++;
	      continue;
	    }
	else //if the equal sign is found,
	  *data = (__internal::stripQuotes(string_t(flag, index + 1)) == "0") ? false : true;
	i++;
	continue;
      }

#define HANDLE_NUMERIC_FLAGS_AS_IS(TYPE_CPP, TYPE, CONVERSION_FUNCTION) /* TYPE_CPP is the actual type in C++ language, TYPE is the representation that is in the enum 'TYPE', and CONVERSION_FUNCTION is either std::stod or std::stoi */ \
	  TYPE_CPP* data = TYPE##_lookup.at(flag_only); \
	  typename string_t::size_type index = 0; \
	  if ((index = flag.find('=')) != std::string::npos) { /* finds an equal */ \
	    *data = CONVERSION_FUNCTION(__internal::stripQuotes(flag.substr(index + 1))); \
	    i++; \
	    continue; \
	  } else /* no equals, implies: second argument exists. If not, invalid. */ \
	    if (i + 1 >= _argc) { /* checks if a next argument exists */ \
	    invalid_flags.push_back(flag); \
	    i++; \
	    continue; \
	    } else { /* forcefully take the next argument. (possible to get invalid_argument */ \
	      *data = CONVERSION_FUNCTION(__internal::stripQuotes(string_t(_argv[i + 1]))); \
	      i += 2; \
	      continue; \
	    }

      case FLOAT: {
	HANDLE_NUMERIC_FLAGS_AS_IS(floating_t, float, std::stod)
	break;
      }

      case INT: {
	HANDLE_NUMERIC_FLAGS_AS_IS(integer_t, int, std::stoi)
	break;
      }

      case STR: {
	string_t* data = str_lookup.at(flag_only);
	typename string_t::size_type index = 0;
	if ((index = flag.find('=')) != std::string::npos) { //check for existance of equals
	  *data = __internal::stripQuotes(flag.substr(index + 1));
	  i++;
	  continue;
	} else //equals does not exists
	  if (i + 1 >= _argc) {
	    invalid_flags.push_back(flag);
	    i++;
	    continue;
	  } else {
	    *data = __internal::stripQuotes(string_t(_argv[i + 1]));
	    i += 2;
	    continue;
	  }
      }

      default: {
	throw impossible_case_reached("Impossible case reached in __internal_readArgs_AS_IS().");
      }
      }
    }
  }

  // Deletes any new allocations done by this function
  for (auto && it : allocs)
    delete[] it; //deletes any NEW allocations done by function

  if (_argv != _argv_backup) {
    delete[] _argv; //delete current _argv
    _argv = _argv_backup; //restore argv's backup
  }

  allocs.clear();

  return invalid_flags;
}

using GetOpt = __impl_GetOpt<int, char, double, std::string>;
using uGetOpt = __impl_GetOpt<unsigned int, unsigned char, long double, std::string>;
