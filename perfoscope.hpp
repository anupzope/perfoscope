#ifndef _PERFOSCOPE_HPP_
#define _PERFOSCOPE_HPP_

#include "texttablefwd.hpp"
#include "common.hpp"

#ifdef USING_PERFOSCOPE_HWC
#ifdef USING_PAPI
#include <papi.h>
#else // USING_PAPI
#error "PAPI not configured..."
#endif // USING_PAPI
#endif // USING_PERFOSCOPE_HWC

#ifdef USING_PERFOSCOPE_DBSTORE
#ifdef USING_SQLITE
#include <sqlite3.h>
#else
#error "sqlite3 not configured..."
#endif // USING_SQLITE3
#endif // USING_PERFOSCOPE_DBSTORE

#include <string>
#include <vector>
#include <sstream>

/**---------------------------------------------------------------------------*/

class PerfoscopeData;
class Perfoscope;

/**---------------------------------------------------------------------------*/

class PerfoscopeUtil {
public:
  static const PerfoscopeData& init(
    const char *profile, 
    const char *categories[], 
    const int ncategories, 
    const char *events[], 
    const int nevents, 
    const char *dbfilename = nullptr, 
    const char *dbvfs = nullptr, 
    const char *file = "\0", const int line = 0); // main, sync
  
  static void finalize(const char *file = "\0", const int line = 0); // main, sync
  
  static void add_run_data(
    const PerfoscopeData* perfoscope_data_list[], 
    const int count, 
    const int problem_size = -1); // main, sync
  
  template<typename... Targs>
  static void print_error(const char *file, const int line, 
      const char *format, Targs... args) {
    std::stringstream strm;
    strm << "Perfoscope error (%s, %d): " << format << "\n";
    fprintf(stderr, strm.str().c_str(), file, line, args...);
  }
  
#ifdef USING_PERFOSCOPE_DBSTORE
  static int open_sqlite3db(); // main, sync
  
  static int close_sqlite3db(); // main, sync
  
  static int load_sqlite3db(); // main, sync
  
  static int store_sqlite3db(); // main, sync
  
  static int create_perfoscope_data_schema(); // main, sync
  
  static int insert_perfoscope_data_profile(const PerfoscopeData &data); // main, sync
  
  static void add_perfoscope_data(const PerfoscopeData &data, long long run_id); // main, sync
#endif // USING_PERFOSCOPE_DBSTORE
  
private:
#ifdef USING_PERFOSCOPE_DBSTORE
  static int check_if_perfoscope_data_profile_exists(
    const PerfoscopeData &data, int *exists
  ); // main
  
  static int create_table_perf_profile(); // main
  
  static int insert_into_perf_profile(const char *profile_name); // main
  
  static int create_table_perf_category(); // main
  
  static int insert_into_perf_category(const char *category_name); // main
  
  static int create_table_perf_event(); // main
  
  static int insert_into_perf_event(
    const char *profile_name, 
    const char *event_name
  ); // main
  
  static int insert_if_not_exists_into_perf_event(
    const char *profile_name, 
    const char *event_name
  ); // main
  
  static int create_table_perf_run(); // main
  
  static int create_new_run(
    const char *profile_name, 
    const long long problem_size, 
    long long *run_id
  ); // main
  
  static int create_table_perf_value(); // main
  
  static int insert_into_perf_value(
    int proc_id, 
    int thread_id, 
    const char *profile_name, 
    const char *category_name, 
    const char *event_name, 
    long long run_id, 
    long long value
  ); // main
  
  static int insert_into_perf_value(
    int proc_id, 
    int thread_id, 
    const char *profile_name, 
    const char *category_name, 
    const char *event_name, 
    long long run_id, 
    double value
  ); // main
  
  static int insert_perfoscope_data(
    int proc_id, 
    const PerfoscopeData &data, 
    long long run_id, 
    const long long *counter_values, 
    const double *real_time
  ); // main
  
  static void extract_perfoscope_data_to_arrays(
    const PerfoscopeData &data, 
    long long *counter_values, 
    double *real_time
  ); // main
#endif // USING_PERFOSCOPE_DBSTORE
  
private:
#ifdef USING_PERFOSCOPE_DBSTORE
  static const char * get_dbfilename() {
    return (s_dbfilename.length() == 0 ? "perf.db" : s_dbfilename.c_str());
  }
  
  static const char * get_dbvfs() {
    return (s_dbvfs.length() == 0 ? nullptr : s_dbvfs.c_str());
  }
#endif
  
private:
  static bool s_initialized;
  static bool s_modified;
  static int s_owner_proc_id;
  static PerfoscopeData s_template;
#ifdef USING_PERFOSCOPE_DBSTORE
  static std::string s_dbfilename;
  static std::string s_dbvfs;
  static sqlite3 *s_sqldb;
  static bool s_forkeyon;
  static const char *s_create_new_run_query;
  static sqlite3_stmt *s_create_new_run_stmt;
  static const char *s_insert_value_query;
  static sqlite3_stmt *s_insert_value_stmt;
#endif // #ifdef USING_PERFOSCOPE_DBSTORE
};

/**---------------------------------------------------------------------------*/

class PerfoscopeData {
  friend class Perfoscope;
  friend class PerfoscopeUtil;
  
private:
  struct CategoryData {
    CategoryData()
#ifdef USING_PERFOSCOPE_WCT
      : real_time(0)
#endif // USING_PERFOSCOPE_WCT
    {}
    
    CategoryData(std::string name) : name(name)
#ifdef USING_PERFOSCOPE_WCT
      , real_time(0)
#endif // USING_PERFOSCOPE_WCT
    {}
    
    CategoryData(const CategoryData &rhs) : name(rhs.name)
#ifdef USING_PERFOSCOPE_HWC
      , counter_values(rhs.counter_values)
#endif // #ifdef USING_PERFOSCOPE_HWC
#ifdef USING_PERFOSCOPE_WCT
      , real_time(rhs.real_time)
#endif // #ifdef USING_PERFOSCOPE_WCT
    {}
    
    CategoryData & operator=(const CategoryData &rhs) {
      name = rhs.name;
      
#ifdef USING_PERFOSCOPE_HWC
      counter_values = rhs.counter_values;
#endif // USING_PERFOSCOPE_HWC
      
#ifdef USING_PERFOSCOPE_WCT
      real_time = rhs.real_time;
#endif // USING_PERFOSCOPE_WCT
      
      return *this;
    }
    
#ifdef USING_PERFOSCOPE_WCT
    double real_time;
#endif // USING_PERFOSCOPE_WCT
    
#ifdef USING_PERFOSCOPE_HWC
    std::vector<long long> counter_values;
#endif // USING_PERFOSCOPE_HWC
    
    std::string name;
  };
  
public:
  PerfoscopeData() : m_thread_id(-1) {}
  
  ~PerfoscopeData() {}
  
  std::string profile_name() const {
    return m_profile_name;
  }
  
  int thread_id() const {
    return m_thread_id;
  }
  
  int categories_count() const {
    return m_category_data.size();
  }
  
  std::string category_name(const int ci) const {
    return m_category_data[ci].name;
  }
  
  const long long * category_values(const int ci) const {
#ifdef USING_PERFOSCOPE_HWC
    return &m_category_data[ci].counter_values[0];
#else // USING_PERFOSCOPE_HWC
    return nullptr;
#endif // USING_PERFOSCOPE_HWC
  }
  
  double category_real_time(const int ci) const {
#ifdef USING_PERFOSCOPE_WCT
    return m_category_data[ci].real_time;
#else // USING_PERFOSCOPE_WCT
    return 0.0;
#endif // USING_PERFOSCOPE_WCT
  }
  
  void reset_counter_values() {
    int ncategories = m_category_data.size();
    for(int i = 0; i < ncategories; ++i) {
#ifdef USING_PERFOSCOPE_HWC
      std::vector<long long> &counter_values = m_category_data[i].counter_values;
      int ncounter_values = counter_values.size();
      for(int j = 0; j < ncounter_values; ++j) {
        counter_values[j] = 0;
      }
#endif // #ifdef USING_PERFOSCOPE_HWC
      m_category_data[i].real_time = 0.0;
    }
  }
  
  void reset_counter_values(const int ci) {
#ifdef USING_PERFOSCOPE_HWC
    std::vector<long long> &counter_values = m_category_data[ci].counter_values;
    int ncounter_values = counter_values.size();
    for(int j = 0; j < ncounter_values; ++j) {
      counter_values[j] = 0;
    }
#endif // #ifdef USING_PERFOSCOPE_HWC
    m_category_data[ci].real_time = 0.0;
  }
  
  int events_count() const {
#ifdef USING_PERFOSCOPE_HWC
    return m_event_codes.size();
#else // USING_PERFOSCOPE_HWC
    return 0;
#endif // USING_PERFOSCOPE_HWC
  }
  
  std::string event_name(const int ei, const char *file = "\0", const int line = 0) const {
#ifdef USING_PERFOSCOPE_HWC
    char eventname[PAPI_MAX_STR_LEN];
    int errcode = PAPI_event_code_to_name(m_event_codes[ei], eventname);
    if(errcode != PAPI_OK) {
      PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
        __PRETTY_FUNCTION__, "could not convert event code to name", errcode, PAPI_strerror(errcode));
      perfoscope_internal::abort(errcode);
    }
    return std::string(eventname);
#else // USING_PERFOSCOPE_HWC
    return "";
#endif // USING_PERFOSCOPE_HWC
  }
  
  PerfoscopeData * clone(int thread_id) const {
    PerfoscopeData *pobj = new PerfoscopeData();
    
    pobj->m_profile_name = m_profile_name;
    pobj->m_thread_id = thread_id;
    
    int ncategories = m_category_data.size();
    pobj->m_category_data.resize(ncategories);
    for(int ci = 0; ci < ncategories; ++ci) {
      pobj->m_category_data[ci].name = m_category_data[ci].name;
#ifdef USING_PERFOSCOPE_HWC
      int nevents = m_event_codes.size();
      pobj->m_event_codes = m_event_codes;
      
      std::vector<long long> &values = pobj->m_category_data[ci].counter_values;
      values.resize(nevents);
      for(int ei = 0; ei < nevents; ++ei) {
        values[ei] = 0;
      }
#endif // #ifdef USING_PERFOSCOPE_HWC
#ifdef USING_PERFOSCOPE_WCT
      pobj->m_category_data[ci].real_time = 0.0;
#endif // #ifdef USING_PERFOSCOPE_WCT
    }
    
    return pobj;
  }
  
private:
  PerfoscopeData(const PerfoscopeData& rhs) = delete;
  PerfoscopeData & operator=(const PerfoscopeData& rhs) = delete;
  
  void profile_name(std::string profile_name) {
    m_profile_name = profile_name;
  }
  
  void thread_id(int thread_id) {
    m_thread_id = thread_id;
  }
  
  int add_category(std::string name) {
    int index = m_category_data.size();
    m_category_data.push_back(CategoryData(name));
    return index;
  }
  
  void clear_categories() {
    m_category_data.clear();
  }
  
  void add_event(std::string event_name, const char *file = "\0", const int line = 0) {
#ifdef USING_PERFOSCOPE_HWC
    int eventcode;
    int errcode = PAPI_event_name_to_code(const_cast<char*>(event_name.c_str()), &eventcode);
    if(errcode != PAPI_OK) {
      PerfoscopeUtil::print_error(file, line, 
        "%s - event %s not found, PAPI errorcode: %d, PAPI error: %s", 
        __PRETTY_FUNCTION__, event_name.c_str(), errcode, PAPI_strerror(errcode));
      perfoscope_internal::abort(errcode);
    }
    m_event_codes.push_back(eventcode);
#endif // USING_PERFOSCOPE_HWC
  }
  
  void clear_events() {
#ifdef USING_PERFOSCOPE_HWC
    m_event_codes.clear();
#endif // #ifdef USING_PERFOSCOPE_HWC
  }

private:
  std::vector<CategoryData> m_category_data;
  std::string m_profile_name;
  int m_thread_id;
#ifdef USING_PERFOSCOPE_HWC
  std::vector<int> m_event_codes;
#endif // USING_PERFOSCOPE_HWC
};

/**---------------------------------------------------------------------------*/

class Perfoscope {
public:
  Perfoscope(PerfoscopeData *data) : 
    m_data(data)
#ifdef USING_PERFOSCOPE_WCT
    , m_real_time({0,0})
#endif // USING_PERFOSCOPE_WCT
#ifdef USING_PERFOSCOPE_HWC
    , m_eventset(PAPI_NULL)
#endif // USING_PERFOSCOPE_HWC
  {}
  
  Perfoscope(const Perfoscope &rhs) : 
    m_data(rhs.m_data)
#ifdef USING_PERFOSCOPE_WCT
    , m_real_time(rhs.m_real_time)
#endif // USING_PERFOSCOPE_WCT
#ifdef USING_PERFOSCOPE_HWC
    , m_eventset(rhs.m_eventset)
#endif // USING_PERFOSCOPE_HWC
  {}
  
  ~Perfoscope() {}
  
  Perfoscope & operator=(const Perfoscope &rhs) {
    m_data = rhs.m_data;
    
#ifdef USING_PERFOSCOPE_WCT
    m_real_time = rhs.m_real_time;
#endif // USING_PERFOSCOPE_WCT
    
#ifdef USING_PERFOSCOPE_HWC
    m_eventset = rhs.m_eventset;
#endif // USING_PERFOSCOPE_HWC
    
    return *this;
  }
  
  void init(const char *file = "\0", const int line = 0);
  
  void start(const char *file = "\0", const int line = 0);
  
  void reset(const char *file = "\0", const int line = 0);
  
  void accumulate(const int ci = 0, const char *file = "\0", const int line = 0);
  
  void stop(const int ci = 0, const char *file = "\0", const int line = 0);
  
  void stop(const char *file = "\0", const int line = 0);
  
  void destroy(const char *file = "\0", const int line = 0);
  
private:
  PerfoscopeData *m_data;
  
#ifdef USING_PERFOSCOPE_HWC
  int m_eventset;
#endif // USING_PERFOSCOPE_HWC
  
#ifdef USING_PERFOSCOPE_WCT
  perfoscope_internal::real_time_t m_real_time;
#endif // USING_PERFOSCOPE_WCT
};

#ifndef NO_PERFOSCOPE

// OpenMP wrappers
#ifdef _OPENMP

#include <omp.h>

extern PerfoscopeData **all_pscope_data;
extern int all_pscope_data_count;

Perfoscope *pscope;
#pragma omp threadprivate(pscope)

inline void perfoscope_init(char *profile_name, char *categories[], int ncategories, char *events[], int nevents) {
  const PerfoscopeData & tmplt = PerfoscopeUtil::init(profile_name, categories, ncategories, events, nevents);
  all_pscope_data_count = omp_get_max_threads();
  all_pscope_data = new PerfoscopeData*[all_pscope_data_count];
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    all_pscope_data[tid] = tmplt.clone(tid);
    pscope = new Perfoscope(all_pscope_data[tid]);
    
    pscope->init(__FILE__, __LINE__);
    pscope->start(__FILE__, __LINE__);
    pscope->stop(__FILE__, __LINE__);
    pscope->start(__FILE__, __LINE__);
  }
}

inline void perfoscope_reset_counters() {
  pscope->reset();
}

inline void perfoscope_accumulate_counters(int category_id) {
  pscope->accumulate(category_id);
}

inline void perfoscope_add(int problem_size = -1) {
  PerfoscopeUtil::add_run_data(all_pscope_data, all_pscope_data_count, problem_size);
}

inline void perfoscope_clear() {
  #pragma omp parallel
  {
    all_pscope_data[omp_get_thread_num()]->reset_counter_values();
  }
}

inline void perfoscope_finalize() {
  #pragma omp parallel
  {
    pscope->stop(__FILE__, __LINE__);
    pscope->destroy(__FILE__, __LINE__);
    delete pscope;
  }
  PerfoscopeUtil::finalize(__FILE__, __LINE__);
  delete[] all_pscope_data;
}

#endif // #ifdef _OPENMP

#else // #ifndef NO_PERFOSCOPE

#define perfoscope_init(profile_name, categories, ncategories, events, nevents)
#define perfoscope_reset_counters()
#define perfoscope_accumulate_counters(category_id)
#define perfoscope_add(problem_size)
#define perfoscope_clear()
#define perfoscope_finalize()

#endif // #ifndef NO_PERFOSCOPE

/**---------------------------------------------------------------------------*/

TextTable * create_texttable(const PerfoscopeData &data);

/**---------------------------------------------------------------------------*/

#endif // #ifndef _PERFOSCOPE_HPP_
