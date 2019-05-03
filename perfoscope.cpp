#include "perfoscope.hpp"
#include "texttable.hpp"

#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdio>

/**---------------------------------------------------------------------------*/

bool PerfoscopeUtil::s_initialized = false;
bool PerfoscopeUtil::s_modified = false;
int PerfoscopeUtil::s_owner_proc_id = 0;
PerfoscopeData PerfoscopeUtil::s_template;
#ifdef USING_PERFOSCOPE_DBSTORE
std::string PerfoscopeUtil::s_dbfilename;
std::string PerfoscopeUtil::s_dbvfs;
sqlite3 *PerfoscopeUtil::s_sqldb = nullptr;
bool PerfoscopeUtil::s_forkeyon;

const char * PerfoscopeUtil::s_create_new_run_query = 
"insert into perf_run (run, size, profile_id) "
"values ("
"(select ifnull(max(r.run+1), 1) from perf_run r, perf_profile p where p.name=?2 and r.profile_id=p.id and r.size=?1), "
"?1, "
"(select p.id from perf_profile p where p.name=?2));";
sqlite3_stmt * PerfoscopeUtil::s_create_new_run_stmt = nullptr;

const char * PerfoscopeUtil::s_insert_value_query = 
"insert into perf_value(proc_id, thread_id, profile_id, category_id, event_id, run_id, value) "
"select ?1 as proc_id, ?2 as thread_id, p.id, c.id, e.id, ?3 as run_id, ?4 as value "
"from perf_profile p, perf_category c, perf_event e "
"where p.name=?5 and c.name=?6 and e.name=?7 and e.profile_id=p.id;";
sqlite3_stmt * PerfoscopeUtil::s_insert_value_stmt = nullptr;
#endif // #ifdef USING_PERFOSCOPE_DBSTORE

const PerfoscopeData& PerfoscopeUtil::init(
    const char *profile,
    const char *categories[],
    const int ncategories,
    const char *events[],
    const int nevents,
    const char *dbfilename, 
    const char *dbvfs, 
    const char *file, 
    const int line) {
  if(!s_initialized) {
    s_initialized = true;
    s_modified = false;
    
    s_template.clear_events();
    s_template.clear_categories();
    s_template.profile_name(profile);
    for(int i = 0; i < ncategories; ++i) {
      s_template.add_category(categories[i]);
    }
    
    int iproc = perfoscope_internal::iproc();
    int nproc = perfoscope_internal::nproc();
    int *int_recvbuf = new int[nproc];
    
#ifdef USING_PERFOSCOPE_DBSTORE
    s_dbfilename = (dbfilename == nullptr ? "perf.db" : dbfilename);
    s_dbvfs = (dbvfs == nullptr ? "unix-none" : dbvfs);
#endif // #ifdef USING_PERFOSCOPE_DBSTORE
    
#ifdef USING_PERFOSCOPE_HWC
    {
      int errcode;
      errcode = PAPI_library_init(PAPI_VER_CURRENT);
      if(errcode != PAPI_VER_CURRENT && errcode > 0) {
        print_error(file, line, "%s - Could not initialize PAPI on process %d, PAPI errorcode: %d, PAPI error: %s", 
          __PRETTY_FUNCTION__, iproc, errcode, PAPI_strerror(errcode));
      } else {
        errcode = PAPI_thread_init(pthread_self);
        if(errcode != PAPI_OK) {
          print_error(file, line, "%s - Could not initialize PAPI thread support on process %d, PAPI errorcode: %d, PAPI error: %s", 
            __PRETTY_FUNCTION__, iproc, errcode, PAPI_strerror(errcode));
        }
      }
      
#ifdef USING_MPIC
      MPI_Alltoall(&errcode, 1, MPI_INT, int_recvbuf, nproc, MPI_INT, MPI_COMM_WORLD);
#else // #ifdef USING_MPIC
      int_recvbuf[0] = errcode;
#endif // #ifdef USING_MPIC
      for(int i = 0; i < nproc; ++i) {
        if(int_recvbuf[i] != PAPI_OK) {
          perfoscope_internal::abort(int_recvbuf[i]);
        }
      }
    }
    
    for(int i = 0; i < nevents; ++i) {
      s_template.add_event(events[i]);
    }
#endif // #ifdef USING_PERFOSCOPE_HWC
    
#ifdef USING_MPIC
    {
      char char_recvbuf[4096];
      
      std::string profile_name = s_template.profile_name();
      if(iproc == s_owner_proc_id) {
        std::strcpy(char_recvbuf, profile_name.c_str());
      }
      MPI_Bcast(char_recvbuf, profile_name.length()+1, MPI_CHAR, s_owner_proc_id, MPI_COMM_WORLD);
      if(std::strcmp(profile_name.c_str(), char_recvbuf) != 0) {
        print_error(file, line, "%s - Profile name does not match on process %d", __PRETTY_FUNCTION__, iproc);
        perfoscope_internal::abort(-1);
      }
      
      int events_count = s_template.events_count();
      MPI_Alltoall(&events_count, 1, MPI_INT, int_recvbuf, nproc, MPI_INT, MPI_COMM_WORLD);
      for(int i = 0; i < nproc; ++i) {
        if(int_recvbuf[i] != events_count) {
          print_error(file, line, "%s - Number of events do not match on process %d", __PRETTY_FUNCTION__, i);
          perfoscope_internal::abort(-1);
        }
      }
      
      for(int i = 0; i < events_count; ++i) {
        std::string event_name = s_template.event_name(i);
        if(iproc == s_owner_proc_id) {
          std::strcpy(char_recvbuf, event_name.c_str());
        }
        MPI_Bcast(char_recvbuf, event_name.length()+1, MPI_CHAR, s_owner_proc_id, MPI_COMM_WORLD);
        if(std::strcmp(event_name.c_str(), char_recvbuf) != 0) {
          print_error(file, line, "%s - Event name does not match on process %d", __PRETTY_FUNCTION__, iproc);
          perfoscope_internal::abort(-1);
        }
      }
      
      int categories_count = s_template.categories_count();
      MPI_Alltoall(&categories_count, 1, MPI_INT, int_recvbuf, nproc, MPI_INT, MPI_COMM_WORLD);
      for(int i = 0; i < nproc; ++i) {
        if(int_recvbuf[i] != categories_count) {
          print_error(file, line, "%s - Number of categories do not match on process %d", __PRETTY_FUNCTION__, iproc);
          perfoscope_internal::abort(-1);
        }
      }
      
      for(int i = 0; i < categories_count; ++i) {
        std::string category_name = s_template.category_name(i);
        if(iproc == s_owner_proc_id) {
          std::strcpy(char_recvbuf, category_name.c_str());
        }
        MPI_Bcast(char_recvbuf, category_name.length()+1, MPI_CHAR, s_owner_proc_id, MPI_COMM_WORLD);
        if(std::strcmp(category_name.c_str(), char_recvbuf) != 0) {
          print_error(file, line, "%s - Category name does not match on process %d", __PRETTY_FUNCTION__, iproc);
          perfoscope_internal::abort(-1);
        }
      }
    }
#endif // #ifdef USING_MPIC
    
#ifdef USING_PERFOSCOPE_DBSTORE
    {
      int sqlrc;
      if((sqlrc = open_sqlite3db()) != SQLITE_OK) {
        print_error(file, line, "Could not create perfdata data store (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        perfoscope_internal::abort(sqlrc);
      }
      
      load_sqlite3db();
      
      if((sqlrc = create_perfoscope_data_schema()) != SQLITE_OK) {
        print_error(file, line, "Could not create perfdata schema (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        perfoscope_internal::abort(sqlrc);
      }
      
      if((sqlrc = insert_perfoscope_data_profile(s_template)) != SQLITE_OK) {
        print_error(file, line, "Could not create perfdata profile (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        perfoscope_internal::abort(sqlrc);
      }
      
      if((sqlrc = sqlite3_prepare_v2(s_sqldb, s_create_new_run_query, -1, &s_create_new_run_stmt, NULL)) != SQLITE_OK) {
        print_error(file, line, "Could not create statement for creating new perfdata run (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        print_error(file, line, "Query: %s", s_create_new_run_query);
        perfoscope_internal::abort(sqlrc);
      }
      
      if((sqlrc = sqlite3_prepare_v2(s_sqldb, s_insert_value_query, -1, &s_insert_value_stmt, NULL)) != SQLITE_OK) {
        print_error(file, line, "Could not create statement for inserting perfdata value (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        print_error(file, line, "Query: %s", s_insert_value_query);
        perfoscope_internal::abort(sqlrc);
      }
    }
#endif // #ifdef USING_PERFOSCOPE_DBSTORE
    
#ifdef USING_MPIC
    delete[] int_recvbuf;
#endif // #ifdef USING_MPIC
  }
  
  return s_template;
}

void PerfoscopeUtil::finalize(const char *file, const int line) {
  if(s_initialized) {
#ifdef USING_PERFOSCOPE_DBSTORE
    if(s_modified) {
      store_sqlite3db();
      s_modified = false;
    } else {
      print_error(file, line, "Skipping writing of performance data since there is no modified data");
    }
    
    sqlite3_finalize(s_create_new_run_stmt);
    s_create_new_run_stmt = nullptr;
    
    sqlite3_finalize(s_insert_value_stmt);
    s_insert_value_stmt = nullptr;
    
    close_sqlite3db();
#endif // #ifdef USING_PERFOSCOPE_DBSTORE
    s_initialized = false;
  }
}

void PerfoscopeUtil::add_run_data(
    const PerfoscopeData* perfoscope_data_list[], 
    const int count, 
    const int problem_size) {
  // Write to flat file
//  std::stringstream perfdata_ffname;
//  perfdata_ffname << dbcase << "_p" << perfoscope_internal::iproc() << ".txt";
//  
//  std::ofstream perfdata_ffile(perfdata_ffname.str().c_str());
//  for(size_t pdi = 0; pdi < count; ++pdi) {
//    perfdata_ffile << "Profile: " << perfoscope_data_list[pdi]->profile_name();
//    perfdata_ffile << ", Thread: " << perfoscope_data_list[pdi]->thread_id();
//    //perfdata_ffile << ", nruns: " << nruns;
//    perfdata_ffile << std::endl;
//    TextTable *table = create_texttable(perfoscope_data_list[pdi]);
//    perfdata_ffile << *table << std::endl;
//    delete table;
//  }
//  perfdata_ffile.close();
  
#ifdef USING_PERFOSCOPE_DBSTORE
  int sqlrc;
  long long run_id;
  bool run_created = false;
  
  for(int i = 0; i < count; ++i) {
    if(perfoscope_data_list[i] != nullptr) {
      if((sqlrc = create_new_run(perfoscope_data_list[i]->profile_name().c_str(), problem_size, &run_id)) == SQLITE_OK) {
        run_created = true;
        s_modified = true;
        break;
      } else {
        print_error(__FILE__, __LINE__, "Failed to create a new run (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
      }
    }
  }
  
  if(run_created) {
    for(int i = 0; i < count; ++i) {
      if(perfoscope_data_list[i] != nullptr) {
        add_perfoscope_data(*perfoscope_data_list[i], run_id);
      }
    }
  }
#endif // #ifdef USING_PERFOSCOPE_DBSTORE
}

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::open_sqlite3db() {
  s_sqldb = nullptr;
  int sqlrc = SQLITE_ERROR;
  
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    char *sqlem;
    
    if((sqlrc = sqlite3_open(":memory:", &s_sqldb)) != SQLITE_OK) {
      print_error(__FILE__, __LINE__, "Could not open database (error: %s, code: %d)", 
        sqlite3_errstr(sqlrc), sqlrc);
      s_sqldb = nullptr;
    }
    
    if(s_sqldb != nullptr) {
      if((sqlrc = sqlite3_exec(s_sqldb, "PRAGMA foreign_keys = on;", NULL, NULL, &sqlem)) == SQLITE_OK) {
        s_forkeyon = true;
      } else {
        s_forkeyon = false;
        print_error(__FILE__, __LINE__, "Cound not enforce foreign key constraint: %s", sqlem);
        sqlite3_free(sqlem);
      }
    }
    
    sqlrc = (s_sqldb == nullptr ? SQLITE_ERROR : SQLITE_OK);
  }
#ifdef USING_MPIC
  MPI_Bcast(&sqlrc, 1, MPI_INT, s_owner_proc_id, MPI_COMM_WORLD);
#endif // USING_MPIC
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::close_sqlite3db() {
  int sqlrc = SQLITE_ERROR;
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    sqlrc = sqlite3_close(s_sqldb);
  }
#ifdef USING_MPIC
  MPI_Bcast(&sqlrc, 1, MPI_INT, s_owner_proc_id, MPI_COMM_WORLD);
#endif // USING_MPIC
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::load_sqlite3db() {
  int sqlrc = SQLITE_ERROR;
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    sqlite3 *filedb;
    if((sqlrc = sqlite3_open_v2(get_dbfilename(), &filedb, SQLITE_OPEN_READONLY, get_dbvfs())) == SQLITE_OK) {
      fprintf(stdout, "Reading sqlite3 db from file '%s'\n", get_dbfilename());
      sqlite3_backup *backup;
      if((backup = sqlite3_backup_init(s_sqldb, "main", filedb, "main"))) {
        if((sqlrc = sqlite3_backup_step(backup, -1)) != SQLITE_DONE) {
          fprintf(stdout, "Could not read sqlite3 db from file '%s' (error: %s, code: %d)\n", 
            get_dbfilename(), sqlite3_errstr(sqlrc), sqlrc);
        } else {
          fprintf(stdout, "Done reading sqlite3 db from file '%s'\n", get_dbfilename());
        }
        sqlite3_backup_finish(backup);
      } else {
        sqlrc = sqlite3_errcode(s_sqldb);
        fprintf(stdout, "Could not read sqlite3 db from file '%s' (error: %s, code: %d)\n", 
          get_dbfilename(), sqlite3_errstr(sqlrc), sqlrc);
      }
    } else {
      fprintf(stdout, "Could not read sqlite3 db from file '%s' (error: %s, code: %d)\n", 
        get_dbfilename(), sqlite3_errstr(sqlrc), sqlrc);
    }
    sqlite3_close(filedb);
  }
#ifdef USING_MPIC
  MPI_Bcast(&sqlrc, 1, MPI_INT, s_owner_proc_id, MPI_COMM_WORLD);
#endif // USING_MPIC
  return sqlrc;
}
#endif

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::store_sqlite3db() {
  int sqlrc = SQLITE_ERROR;
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    sqlite3 *filedb;
    if((sqlrc = sqlite3_open_v2(get_dbfilename(), &filedb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, get_dbvfs())) == SQLITE_OK) {
      fprintf(stdout, "Writing sqlite3 db to file '%s'\n", get_dbfilename());
      sqlite3_backup *backup;
      backup = sqlite3_backup_init(filedb, "main", s_sqldb, "main");
      if(backup) {
        sqlrc = sqlite3_backup_step(backup, -1);
        if(sqlrc != SQLITE_DONE) {
          print_error(__FILE__, __LINE__, "Error writing sqlite3 db to file '%s' (error: %s, code: %d)", get_dbfilename(), sqlite3_errstr(sqlrc), sqlrc);
        } else {
          fprintf(stdout, "Done writing sqlite3 db to file '%s'\n", get_dbfilename());
        }
        sqlite3_backup_finish(backup);
      } else {
        sqlrc = sqlite3_errcode(filedb);
        print_error(__FILE__, __LINE__, "Error writing sqlite3 db to file '%s' (error: %s, code: %d)", 
          get_dbfilename(), sqlite3_errstr(sqlrc), sqlrc);
      }
    } else {
      print_error(__FILE__, __LINE__, "Could not open sqlite3 db file '%s' for writing (error: %s, code: %d)", 
        get_dbfilename(), sqlite3_errstr(sqlrc), sqlrc);
    }
    sqlite3_close(filedb);
  }
#ifdef USING_MPIC
  MPI_Bcast(&sqlrc, 1, MPI_INT, s_owner_proc_id, MPI_COMM_WORLD);
#endif // USING_MPIC
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_table_perf_profile() {
  char *query, *sqlem;
  int sqlrc = SQLITE_OK;
  
  query = "create table if not exists perf_profile(id integer primary key autoincrement, name text not null unique);";
  sqlrc = sqlite3_exec(s_sqldb, query, NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not create table 'perf_profile': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", query);
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_into_perf_profile(const char *profile_name) {
  std::stringstream strm;
  int sqlrc = SQLITE_OK;
  char *sqlem;
  
  strm << "insert into perf_profile(name) values('" << profile_name << "');";
  char *query = const_cast<char*>(strm.str().c_str());
  
  sqlrc = sqlite3_exec(s_sqldb, const_cast<char*>(strm.str().c_str()), NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not insert values into table 'perf_profile': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", strm.str().c_str());
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_table_perf_category() {
  char *query, *sqlem;
  int sqlrc = SQLITE_OK;
  
  query = "create table if not exists perf_category("
    "id integer primary key autoincrement, "
    "name text not null unique);";
  
  sqlrc = sqlite3_exec(s_sqldb, query, NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not create table 'perf_category': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", query);
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_into_perf_category(const char *category_name) {
  std::stringstream strm;
  int sqlrc = SQLITE_OK;
  char *sqlem;
  
  strm << "insert into perf_category(name) values(" << "'" << category_name << "');";
  
  sqlrc = sqlite3_exec(s_sqldb, const_cast<char*>(strm.str().c_str()), NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not insert values into table 'perf_category': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", strm.str().c_str());
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_table_perf_event() {
  char *query, *sqlem;
  int sqlrc = SQLITE_OK;
  
  if(s_forkeyon) {
    query = "create table if not exists perf_event("
      "id integer primary key autoincrement, "
      "name text not null, "
      "profile_id integer not null references perf_profile(id), "
      "constraint uk_id unique (name, profile_id));";
  } else {
    query = "create table if not exists perf_event("
      "id integer primary key autoincrement, "
      "name text not null, "
      "profile_id integer not null, "
      "constraint uk_id unique (name, profile_id));";
  }
  
  sqlrc = sqlite3_exec(s_sqldb, query, NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not create table 'perf_event': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", query);
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_into_perf_event(const char *profile_name, const char *event_name) {
  std::stringstream strm;
  int sqlrc = SQLITE_OK;
  char *sqlem;
  
  strm << "insert into perf_event(name, profile_id) values(" << 
    "'" << event_name << "'," <<
    "(select id from perf_profile p where p.name='" << profile_name << "'));";
  
  sqlrc = sqlite3_exec(s_sqldb, const_cast<char*>(strm.str().c_str()), NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not insert values into table 'perf_event': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", strm.str().c_str());
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_table_perf_run() {
  char *query, *sqlem;
  int sqlrc = SQLITE_OK;
  
  if(s_forkeyon) {
    query = "create table if not exists perf_run("
      "id integer primary key autoincrement, "
      "run integer not null, "
      "size integer not null, "
      "profile_id integer not null references perf_profile(id), "
      "constraint uk_id unique(run, size, profile_id));";
  } else {
    query = "create table if not exists perf_run("
      "id integer primary key autoincrement, "
      "run integer not null, "
      "size integer not null, "
      "profile_id integer not null, "
      "constraint uk_id unique(run, size, profile_id));";
  }
  
  sqlrc = sqlite3_exec(s_sqldb, query, NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not create table 'perf_run': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", query);
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_new_run(const char *profile_name, const long long problem_size, long long *run_id) {
  int sqlrc = SQLITE_OK;
  
  if((sqlrc = sqlite3_reset(s_create_new_run_stmt)) == SQLITE_OK) {
    if((sqlrc = sqlite3_bind_int(s_create_new_run_stmt, 1, problem_size)) == SQLITE_OK) {
      if((sqlrc = sqlite3_bind_text(s_create_new_run_stmt, 2, profile_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
        if((sqlrc = sqlite3_step(s_create_new_run_stmt)) == SQLITE_DONE) {
          *run_id = sqlite3_last_insert_rowid(s_sqldb);
          //fprintf(stdout, "run_id: %d\n", *run_id);
          sqlrc = SQLITE_OK;
        }
      }
    }
  }
  
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not create a new run for profile '%s', "
      "(error: %s, code: %d)", profile_name, sqlite3_errstr(sqlrc), sqlrc);
    print_error(__FILE__, __LINE__, "Query: %s with 1:problem_size=%d, 2:profile_name=%s", 
      s_create_new_run_query, problem_size, profile_name);
  }
  
  return sqlrc;
}
#endif // #ifdef USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_table_perf_value() {
  char *query, *sqlem;
  int sqlrc = SQLITE_OK;
  
  if(s_forkeyon) {
    query = "create table if not exists perf_value("
      "id integer primary key autoincrement, "
      "proc_id int not null, "
      "thread_id int not null, "
      "profile_id integer not null references perf_profile(id), "
      "category_id integer not null references perf_category(id), "
      "event_id integer not null references perf_event(id), "
      "run_id integer not null references perf_run(id), "
      "value numeric not null);";
  } else {
    query = "create table if not exists perf_value("
      "id integer primary key autoincrement, "
      "proc_id int not null, "
      "thread_id int not null, "
      "profile_id integer not null, "
      "category_id integer not null, "
      "event_id integer not null, "
      "run_id integer not null, "
      "value numeric not null);";
  }
  
  sqlrc = sqlite3_exec(s_sqldb, query, NULL, NULL, &sqlem);
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not create table 'perf_value': %s", sqlem);
    print_error(__FILE__, __LINE__, "Query: %s", query);
    sqlite3_free(sqlem);
  }
  
  return sqlrc;
}
#endif // #ifdef USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_into_perf_value(int proc_id, int thread_id, 
    const char *profile_name, const char *category_name, const char *event_name, 
    long long run_id, long long value) {
  int sqlrc = SQLITE_OK;
  
  if((sqlrc = sqlite3_reset(s_insert_value_stmt)) == SQLITE_OK) {
    if((sqlrc = sqlite3_bind_int(s_insert_value_stmt, 1, proc_id)) == SQLITE_OK) {
      if((sqlrc = sqlite3_bind_int(s_insert_value_stmt, 2, thread_id)) == SQLITE_OK) {
        if((sqlrc = sqlite3_bind_int64(s_insert_value_stmt, 3, run_id)) == SQLITE_OK) {
          if((sqlrc = sqlite3_bind_int64(s_insert_value_stmt, 4, value)) == SQLITE_OK) {
            if((sqlrc = sqlite3_bind_text(s_insert_value_stmt, 5, profile_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
              if((sqlrc = sqlite3_bind_text(s_insert_value_stmt, 6, category_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
                if((sqlrc = sqlite3_bind_text(s_insert_value_stmt, 7, event_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
                  if((sqlrc = sqlite3_step(s_insert_value_stmt)) == SQLITE_DONE) {
                    sqlrc = SQLITE_OK;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not insert values into table 'perf_value'"
      " (error: %s, code:%d)", sqlite3_errstr(sqlrc), sqlrc);
    print_error(__FILE__, __LINE__, "Query: %s with 1:proc_id=%d, 2:thread_id=%d, "
      "3:run_id=%ll, 4:value=%ll, 5:profile_name=%s, 6:category_name=%s, "
      "7:event_name=%s", s_insert_value_query, proc_id, thread_id, run_id, 
      value, profile_name, category_name, event_name);
  }
  
  return sqlrc;
}
#endif // #ifdef USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_into_perf_value(int proc_id, int thread_id, 
    const char *profile_name, const char *category_name, const char *event_name, 
    long long run_id, double value) {
  int sqlrc = SQLITE_OK;
  
  if((sqlrc = sqlite3_reset(s_insert_value_stmt)) == SQLITE_OK) {
    if((sqlrc = sqlite3_bind_int(s_insert_value_stmt, 1, proc_id)) == SQLITE_OK) {
      if((sqlrc = sqlite3_bind_int(s_insert_value_stmt, 2, thread_id)) == SQLITE_OK) {
        if((sqlrc = sqlite3_bind_int64(s_insert_value_stmt, 3, run_id)) == SQLITE_OK) {
          if((sqlrc = sqlite3_bind_double(s_insert_value_stmt, 4, value)) == SQLITE_OK) {
            if((sqlrc = sqlite3_bind_text(s_insert_value_stmt, 5, profile_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
              if((sqlrc = sqlite3_bind_text(s_insert_value_stmt, 6, category_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
                if((sqlrc = sqlite3_bind_text(s_insert_value_stmt, 7, event_name, -1, SQLITE_STATIC)) == SQLITE_OK) {
                  if((sqlrc = sqlite3_step(s_insert_value_stmt)) == SQLITE_DONE) {
                    sqlrc = SQLITE_OK;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  
  if(sqlrc != SQLITE_OK) {
    print_error(__FILE__, __LINE__, "Could not insert values into table 'perf_value'"
      " (error: %s, code:%d)", sqlite3_errstr(sqlrc), sqlrc);
    print_error(__FILE__, __LINE__, "Query: %s with 1:proc_id=%d, 2:thread_id=%d, "
      "3:run_id=%ll, 4:value=%ll, 5:profile_name=%s, 6:category_name=%s, "
      "7:event_name=%s", s_insert_value_query, proc_id, thread_id, run_id,
      value, profile_name, category_name, event_name);
  }
  
  return sqlrc;
}
#endif // #ifdef USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::check_if_perfoscope_data_profile_exists(const PerfoscopeData &data, int *exist_mask) {
  int sqlrc = SQLITE_OK;
  sqlite3_stmt *stmt;
  const char *query;
  
  *exist_mask = 0;
  
  int categories_count = data.categories_count();
  std::vector<std::string> categories(categories_count);
  for(int i = 0; i < categories_count; ++i) {
    categories[i] = data.category_name(i);
  }
  
  int events_count = data.events_count();
  std::vector<std::string> events(events_count);
  for(int i = 0; i < events_count; ++i) {
    events[i] = data.event_name(i);
  }
#ifdef USING_PERFOSCOPE_WCT
  events_count++;
  events.push_back("time");
#endif // #ifdef USING_PERFOSCOPE_WCT
  
  query = "select name from perf_category;";
  if((sqlrc = sqlite3_prepare_v2(s_sqldb, query, -1, &stmt, NULL)) == SQLITE_OK) {
    while((sqlrc = sqlite3_step(stmt)) == SQLITE_ROW) {
      const unsigned char *catname = sqlite3_column_text(stmt, 0);
      for(std::vector<std::string>::iterator iter = categories.begin(); iter != categories.end(); ++iter) {
        if(std::strcmp(iter->c_str(), (const char*)catname) == 0) {
          categories.erase(iter);
          break;
        }
      }
    }
    sqlite3_finalize(stmt);
    if(sqlrc == SQLITE_DONE) {
      if(categories.size() == 0) {
        sqlrc = SQLITE_OK;
        *exist_mask |= 1;
      } else if(categories.size() == categories_count) {
        sqlrc = SQLITE_OK;
      } else {
        print_error(__FILE__, __LINE__, "Inconsistency in perfdata categories");
        sqlrc = SQLITE_ERROR;
      }
    } else {
      print_error(__FILE__, __LINE__, "Could not check perfdata categories (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
      sqlrc = SQLITE_ERROR;
    }
  } else {
    print_error(__FILE__, __LINE__, "Could not check perfdata categories (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
  }
  if(sqlrc != SQLITE_OK) {
    return sqlrc;
  }
  
  query = "select count(*) from perf_profile where name=?";
  if((sqlrc = sqlite3_prepare_v2(s_sqldb, query, -1, &stmt, NULL)) == SQLITE_OK) {
    if((sqlrc = sqlite3_bind_text(stmt, 1, data.profile_name().c_str(), -1, SQLITE_STATIC)) == SQLITE_OK) {
      if((sqlrc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        if(count == 1) {
          sqlrc = SQLITE_OK;
          *exist_mask |= 2;
        } else if(count == 0) {
          sqlrc = SQLITE_OK;
        } else {
          print_error(__FILE__, __LINE__, "Inconsistency in perfdata profile");
          sqlrc = SQLITE_ERROR;
        }
      } else {
        print_error(__FILE__, __LINE__, "Could not check perfdata profile count (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        sqlrc = SQLITE_ERROR;
      }
      sqlite3_finalize(stmt);
    } else {
      print_error(__FILE__, __LINE__, "Could not check perfdata profile count (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
    }
  } else {
    print_error(__FILE__, __LINE__, "Could not check perfdata profile count (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
  }
  
  if(sqlrc != SQLITE_OK) {
    return sqlrc;
  }
  
  query = "select e.name from perf_profile p, perf_event e where p.name=? and e.profile_id=p.id;";
  if((sqlrc = sqlite3_prepare_v2(s_sqldb, query, -1, &stmt, NULL)) == SQLITE_OK) {
    if((sqlrc = sqlite3_bind_text(stmt, 1, data.profile_name().c_str(), -1, SQLITE_STATIC)) == SQLITE_OK) {
      while((sqlrc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *ename = sqlite3_column_text(stmt, 0);
        for(std::vector<std::string>::iterator iter = events.begin(); iter != events.end(); ++iter) {
          if(std::strcmp(iter->c_str(), (const char*)ename) == 0) {
            events.erase(iter);
            break;
          }
        }
      }
      sqlite3_finalize(stmt);
      if(sqlrc == SQLITE_DONE) {
        if(events.size() == 0) {
          sqlrc = SQLITE_OK;
          *exist_mask |= 4;
        } else if(events.size() == events_count) {
          sqlrc = SQLITE_OK;
        } else {
          print_error(__FILE__, __LINE__, "Inconsistency in perfdata events");
          sqlrc = SQLITE_ERROR;
        }
      } else {
        print_error(__FILE__, __LINE__, "Could not check perfdata events (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
        sqlrc = SQLITE_ERROR;
      }
    } else {
      print_error(__FILE__, __LINE__, "Could not check perfdata events (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
    }
  } else {
    print_error(__FILE__, __LINE__, "Could not check perfdata events (error: %s, code: %d)", sqlite3_errstr(sqlrc), sqlrc);
  }
  
  return sqlrc;
}
#endif // #ifdef USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_perfoscope_data_profile(const PerfoscopeData &data) {
  int sqlrc = SQLITE_ERROR;
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    int exist_mask = 0;
    if((sqlrc = check_if_perfoscope_data_profile_exists(data, &exist_mask)) == SQLITE_OK) {
      if(((exist_mask & 2) >> 1) == ((exist_mask & 4) >> 2)) {
        if((exist_mask & 6) == 0) {
          if((sqlrc = insert_into_perf_profile(data.profile_name().c_str())) == SQLITE_OK) {
            const int nevents = data.events_count();
            for(int ei = 0; ei < nevents; ++ei) {
              if((sqlrc = insert_into_perf_event(data.profile_name().c_str(), data.event_name(ei).c_str())) != SQLITE_OK) {
                break;
              }
            }
#ifdef USING_PERFOSCOPE_WCT
            if(sqlrc == SQLITE_OK) {
              sqlrc = insert_into_perf_event(data.profile_name().c_str(), "time");
            }
#endif // #ifdef USING_PERFOSCOPE_WCT
          }
        }
      } else {
        print_error(__FILE__, __LINE__, "Profile with same name but with different "
          "event set exists in perfdata. Change profile name and try again.");
        sqlrc = SQLITE_ERROR;
      }
      
      if(sqlrc == SQLITE_OK) {
        if((exist_mask & 1) == 0) {
          const int ncategories = data.categories_count();
          for(int ci = 0; ci < ncategories; ++ci) {
            if((sqlrc = insert_into_perf_category(data.category_name(ci).c_str())) != SQLITE_OK) {
              break;
            }
          }
        }
      }
    }
  }
#ifdef USING_MPIC
  MPI_Bcast(&sqlrc, 1, MPI_INT, s_owner_proc_id, MPI_COMM_WORLD);
#endif // USING_MPIC
  return sqlrc;
}
#endif  // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::insert_perfoscope_data(
    int proc_id, 
    const PerfoscopeData &data, 
    long long run_id, 
    const long long *counter_values, 
    const double *real_time) {
  const int ncategories = data.categories_count();
  const int nevents = data.events_count();
  const int array_size = ncategories*nevents;
  int sqlrc = SQLITE_OK;
  
  int cvi = 0, rti = 0;
  for(int ci = 0; ci < ncategories; ++ci) {
#ifdef USING_PERFOSCOPE_HWC
    for(int ei = 0; ei < nevents; ++ei) {
      if((sqlrc = insert_into_perf_value(
        proc_id, 
        data.thread_id(), 
        data.profile_name().c_str(), 
        data.category_name(ci).c_str(), 
        data.event_name(ei).c_str(), 
        run_id, 
        counter_values[cvi++]
      )) != SQLITE_OK) {
        break;
      }
    }
#endif // #ifdef USING_PERFOSCOPE_HWC
    
#ifdef USING_PERFOSCOPE_WCT
    if(sqlrc == SQLITE_OK) {
      sqlrc = insert_into_perf_value(
        proc_id, 
        data.thread_id(), 
        data.profile_name().c_str(), 
        data.category_name(ci).c_str(), 
        "time", 
        run_id, 
        real_time[rti++]
      );
    }
#endif // #ifdef USING_PERFOSCOPE_WCT
  }
  
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
void PerfoscopeUtil::extract_perfoscope_data_to_arrays(
    const PerfoscopeData &data, long long *counter_values, double *real_time) {
  int cvi = 0, rti = 0;
  for(int ci = 0; ci < data.categories_count(); ++ci) {
    const long long *values = data.category_values(ci);
    for(int ei = 0; ei < data.events_count(); ++ei) {
      counter_values[cvi++] = values[ei];
    }
    real_time[rti++] = data.category_real_time(ci);
  }
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
int PerfoscopeUtil::create_perfoscope_data_schema() {
  int sqlrc = SQLITE_OK;
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    if((sqlrc = create_table_perf_profile()) == SQLITE_OK) {
      if((sqlrc = create_table_perf_category()) == SQLITE_OK) {
        if((sqlrc = create_table_perf_event()) == SQLITE_OK) {
          if((sqlrc = create_table_perf_run()) == SQLITE_OK) {
            sqlrc = create_table_perf_value();
          }
        }
      }
    }
  }
#ifdef USING_MPIC
  MPI_Bcast(&sqlrc, 1, MPI_INT, s_owner_proc_id, MPI_COMM_WORLD);
#endif // USING_MPIC
  return sqlrc;
}
#endif // USING_PERFOSCOPE_DBSTORE

#ifdef USING_PERFOSCOPE_DBSTORE
void PerfoscopeUtil::add_perfoscope_data(const PerfoscopeData &data, 
    long long run_id) {
  int sqlrc = SQLITE_OK;
  const int ncategories = data.categories_count();
  const int nevents = data.events_count();
  const int array_size = ncategories*nevents;
  std::vector<long long> counter_values(array_size);
  std::vector<double> real_time(ncategories);
  
  extract_perfoscope_data_to_arrays(data, &counter_values[0], &real_time[0]);
  
  if(perfoscope_internal::iproc() == s_owner_proc_id) {
    sqlrc = insert_perfoscope_data(perfoscope_internal::iproc(), data, run_id, &counter_values[0], &real_time[0]);
    if(sqlrc != SQLITE_OK) {
      print_error(__FILE__, __LINE__, "Error adding perfoscope data to db for process %d (error: %s, code: %d)", 
        perfoscope_internal::iproc(), sqlite3_errstr(sqlrc), sqlrc);
    }
    
#ifdef USING_MPIC
    MPI_Status status;
    for(int pi  = 0; pi < perfoscope_internal::nproc(); ++pi) {
      if(pi != s_owner_proc_id) {
        MPI_Recv(&counter_values[0], array_size, MPI_LONG_LONG, pi, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(&real_time[0], ncategories, MPI_DOUBLE, pi, 1, MPI_COMM_WORLD, &status);
        sqlrc = insert_perfoscope_data(pi, data, run_id, &counter_values[0], &real_time[0]);
        if(sqlrc != SQLITE_OK) {
          print_error(__FILE__, __LINE__, "Error adding perfoscope data to db for process %d (error: %s, code: %d)", 
            pi, sqlite3_errstr(sqlrc), sqlrc);
        }
      }
    }
  } else {
    MPI_Status status;
    MPI_Send(&counter_values[0], array_size, MPI_LONG_LONG, s_owner_proc_id, 0, MPI_COMM_WORLD);
    MPI_Send(&real_time[0], ncategories, MPI_DOUBLE, s_owner_proc_id, 1, MPI_COMM_WORLD);
#endif // USING_MPIC
  }
}
#endif // USING_PERFOSCOPE_DBSTORE

/**---------------------------------------------------------------------------*/

void Perfoscope::init(const char *file, const int line) {
#ifdef USING_PERFOSCOPE_HWC
  int errcode;
  const int nevents = m_data->m_event_codes.size();
  
  // Check if thread id is set
  if(m_data->thread_id() < 0) {
    PerfoscopeUtil::print_error(file, line, "%s - %s", __PRETTY_FUNCTION__, "invalid thread id");
    perfoscope_internal::abort(1);
  }
  
  // Register thread
  errcode = PAPI_register_thread();
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
      __PRETTY_FUNCTION__, "could not register thread", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
  
  // Create eventset
  m_eventset = PAPI_NULL;
  errcode = PAPI_create_eventset(&m_eventset);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
      __PRETTY_FUNCTION__, "could not create eventset", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
  
  // Add events to eventset
  for(int i = 0; i < nevents; ++i) {
    errcode = PAPI_add_event(m_eventset, m_data->m_event_codes[i]);
    if(errcode != PAPI_OK) {
      char ename[PAPI_MAX_STR_LEN];
      PAPI_event_code_to_name(m_data->m_event_codes[i], ename);
      PerfoscopeUtil::print_error(file, line, 
        "%s - %s %s %s, PAPI errorcode: %d, PAPI error: %s", 
        __PRETTY_FUNCTION__, "could not add event", ename, 
        "to eventset", errcode, PAPI_strerror(errcode));
      perfoscope_internal::abort(errcode);
    }
  }
  
  //errcode = PAPI_add_events(m_eventset, &m_data->m_event_codes[0], nevents);
  //if(errcode != PAPI_OK) {
  //  PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
  //    __PRETTY_FUNCTION__, "could not add events to eventset", errcode, PAPI_strerror(errcode));
  //  perfoscope_internal::abort(errcode);
  //}
#endif // USING_PERFOSCOPE_HWC
}

void Perfoscope::start(const char *file, const int line) {
#ifdef USING_PERFOSCOPE_HWC
  int errcode = PAPI_start(m_eventset);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s",
      __PRETTY_FUNCTION__, "could not start PAPI counters", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
#endif // USING_PERFOSCOPE_HWC
  
#ifdef USING_PERFOSCOPE_WCT
  m_real_time = perfoscope_internal::get_real_time();
#endif // USING_PERFOSCOPE_WCT
}

void Perfoscope::reset(const char *file, const int line) {
#ifdef USING_PERFOSCOPE_HWC
  int errcode = PAPI_reset(m_eventset);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s",
      __PRETTY_FUNCTION__, "could not reset PAPI counters", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
#endif // USING_PERFOSCOPE_HWC
  
#ifdef USING_PERFOSCOPE_WCT
  m_real_time = perfoscope_internal::get_real_time();
#endif // USING_PERFOSCOPE_WCT
}

void Perfoscope::accumulate(const int ci, const char *file, const int line) {
#ifdef USING_PERFOSCOPE_WCT
  perfoscope_internal::real_time_t temp = perfoscope_internal::get_real_time();
  m_data->m_category_data[ci].real_time += perfoscope_internal::difftime(temp, m_real_time);
  m_real_time = temp;
#endif // USING_PERFOSCOPE_WCT
  
#ifdef USING_PERFOSCOPE_HWC
  int errcode = PAPI_accum(m_eventset, &m_data->m_category_data[ci].counter_values[0]);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s",
      __PRETTY_FUNCTION__, "could not accumulate PAPI counters", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
#endif // USING_PERFOSCOPE_HWC
}

void Perfoscope::stop(const int ci, const char *file, const int line) {
#ifdef USING_PERFOSCOPE_WCT
  perfoscope_internal::real_time_t temp = perfoscope_internal::get_real_time();
  m_data->m_category_data[ci].real_time += perfoscope_internal::difftime(temp, m_real_time);
  m_real_time = temp;
#endif // USING_PERFOSCOPE_WCT
  
#ifdef USING_PERFOSCOPE_HWC
  int errcode = PAPI_stop(m_eventset, &m_data->m_category_data[ci].counter_values[0]);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s",
      __PRETTY_FUNCTION__, "could not stop PAPI counters", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
#endif // USING_PERFOSCOPE_HWC
}

void Perfoscope::stop(const char *file, const int line) {
#ifdef USING_PERFOSCOPE_HWC
  std::vector<long long> temp(m_data->events_count());
  int errcode = PAPI_stop(m_eventset, &temp[0]);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s",
      __PRETTY_FUNCTION__, "could not stop PAPI counters", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
#endif // USING_PERFOSCOPE_HWC
}

void Perfoscope::destroy(const char *file, const int line) {
#ifdef USING_PERFOSCOPE_HWC
  int errcode;
  
  // Cleanup eventset
  errcode = PAPI_cleanup_eventset(m_eventset);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
      __PRETTY_FUNCTION__, "could not clean up eventset", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
  
  // Destroy eventset
  errcode = PAPI_destroy_eventset(&m_eventset);
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
      __PRETTY_FUNCTION__, "could not destroy eventset", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
  m_eventset = PAPI_NULL;
  
  // Unregister thread
  errcode = PAPI_unregister_thread();
  if(errcode != PAPI_OK) {
    PerfoscopeUtil::print_error(file, line, "%s - %s, PAPI errorcode: %d, PAPI error: %s", 
      __PRETTY_FUNCTION__, "could not unregister thread", errcode, PAPI_strerror(errcode));
    perfoscope_internal::abort(errcode);
  }
#endif // USING_PERFOSCOPE_HWC
}

PerfoscopeData **all_pscope_data = nullptr;
int all_pscope_data_count = 0;

/**---------------------------------------------------------------------------*/

TextTable * create_texttable(const PerfoscopeData &data) {
  int nevents = data.events_count();
  int ncategories = data.categories_count();
  int ei = 0;
  
  TextTable *table = new TextTable(nevents+2, ncategories+1, 2);
  
  for(int ci = 0; ci < ncategories; ++ci) {
    table->at(0, ci+1) = data.category_name(ci);
  }
  
#ifdef USING_PERFOSCOPE_HWC
  for(ei = 0; ei < nevents; ++ei) {
    table->at(ei+1, 0) = data.event_name(ei);
  }
#endif // #ifdef USING_PERFOSCOPE_HWC
  
#ifdef USING_PERFOSCOPE_WCT
  table->at(ei+1, 0) = "time";
#endif // #ifdef USING_PERFOSCOPE_WCT
  
  for(int ci = 0; ci < ncategories; ++ci) {
    ei = 0;
#ifdef USING_PERFOSCOPE_HWC
    const long long *values = data.category_values(ci);
    for(ei = 0; ei < nevents; ++ei) {
      std::stringstream valuestrm;
      valuestrm << values[ei];
      table->at(ei+1, ci+1) = valuestrm.str();
    }
#endif // #ifdef USING_PERFOSCOPE_HWC
#ifdef USING_PERFOSCOPE_WCT
    std::stringstream real_time_strm;
    real_time_strm << data.category_real_time(ci);
    table->at(ei+1, ci+1) = real_time_strm.str();
#endif // #ifdef USING_PERFOSCOPE_WCT
  }
  
  return table;
}

/**---------------------------------------------------------------------------*/

