#include "gdsqlite_database.h"

#include "gdsqlite.config.h"
#include "gdsqlite_statement.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "core/io/file_access.h"

#define ORIGVFS(p) ((sqlite3_vfs*)((p)->pAppData))

void SQLiteDatabase::_bind_methods()
{
    ClassDB::bind_static_method("SQLiteDatabase", D_METHOD("open", "path", "readonly"), &SQLiteDatabase::open);
    ClassDB::bind_method(D_METHOD("close"), &SQLiteDatabase::close);
    ClassDB::bind_method(D_METHOD("prepare", "sql"), &SQLiteDatabase::prepare);
    ClassDB::bind_method(D_METHOD("exec", "sql"), &SQLiteDatabase::exec);
}

namespace
{
    struct VFSFile
    {
        sqlite3_file base_;
        Ref<FileAccess> file_;
    };

    struct VFSInitializer
    {
        sqlite3_vfs vfs_ =
        {
            3, // iVersion
            sizeof(VFSFile), // szOsFile
            1024, // mxPathname
            nullptr, // pNext
            GDSQLITE_VFS_NAME, // zName
            nullptr, // pAppData
            vfs_open, // xOpen
            vfs_delete, // xDelete
            vfs_access, // xAccess
            vfs_fullPathname, // xFullPathname
            vfs_dlopen, // xDlOpen
            vfs_dlerror, // xDlError
            vfs_dlsym, // xDlSym
            vfs_dlclose, // xDlClose
            vfs_randomness, // xRandomness
            vfs_sleep, // xSleep
            vfs_currentTime, // xCurrentTime
            vfs_getLastError, // xGetLastError
            vfs_currentTimeInt64, // xCurrentTimeInt64,
            nullptr, // xSetSystemCall
            nullptr, // xGetSystemCall
            nullptr, // xNextSystemCall
        };

        VFSInitializer()
        {
            sqlite3_vfs_register(&vfs_, 0);
        }

        ~VFSInitializer()
        {
            sqlite3_vfs_unregister(&vfs_);
        }

        static int vfs_open(sqlite3_vfs* pVfs, const char* name, sqlite3_file* file, int flags, int* out_flags)
        {
            static sqlite3_io_methods io_methods_ =
            {
                1, // iVersion
                file_close, // xClose
                file_read, // xRead
                file_write, // xWrite
                file_truncate, // xTruncate
                file_sync, // xSync
                file_fileSize, // xFileSize
                file_lock, // xLock
                file_unlock, // xUnlock
                file_checkReservedLock, // xCheckReservedLock
                file_fileControl, // xFileControl
                file_sectorSize, // xSectorSize
                file_deviceCharacteristics, // xDeviceCharacteristics
            };

            VFSFile* gdfile = (VFSFile*) file;
            gdfile->file_ = FileAccess::open(String::utf8(name), FileAccess::READ_WRITE);
            gdfile->base_.pMethods = &io_methods_;
            return SQLITE_OK;
        }

        static int vfs_delete(sqlite3_vfs* pVfs, const char* name, int syncDir)
        {
            const String path = String::utf8(name);
            const Ref dir = DirAccess::open(path.get_base_dir());
            const Error err = dir->remove(path);
            switch (err)
            {
            case OK: return SQLITE_OK;
            case ERR_FILE_NOT_FOUND: return SQLITE_IOERR_DELETE_NOENT;
            default: return SQLITE_IOERR_DELETE;
            }
        }

        static int vfs_access(sqlite3_vfs* pVfs, const char* name, int flags, int* out_res)
        {
            const String path = String::utf8(name);
            switch (flags)
            {
            case SQLITE_ACCESS_EXISTS:
                {
                    *out_res = FileAccess::exists(path);
                }
                break;
            case SQLITE_ACCESS_READWRITE:
                {
                    Error err;
                    const Ref dummy = FileAccess::open(path, FileAccess::READ_WRITE, &err);
                    *out_res = err == OK;
                }
                break;
            default: // never happen
                *out_res = 0;
                break;
            }
            return SQLITE_OK;
        }

        static int vfs_fullPathname(sqlite3_vfs* pVfs, const char* name, int n_out, char* out)
        {
            sqlite3_snprintf(n_out, out, "%s", name);
            return SQLITE_OK;
        }

        static void* vfs_dlopen(sqlite3_vfs* pVfs, const char* name)
        {
            return nullptr;
        }

        static void vfs_dlerror(sqlite3_vfs* pVfs, int n_out, char* out)
        {
            sqlite3_snprintf(n_out, out, "dlopen not supported");
        }

        static void (*vfs_dlsym(sqlite3_vfs* pVfs, void* handle, const char* symbol))(void)
        {
            return nullptr;
        }

        static void vfs_dlclose(sqlite3_vfs* pVfs, void* handle)
        {
        }

        static int vfs_randomness(sqlite3_vfs* pVfs, int n_out, char* out)
        {
            return ORIGVFS(pVfs)->xRandomness(ORIGVFS(pVfs), n_out, out);
        }

        static int vfs_sleep(sqlite3_vfs* pVfs, int microseconds)
        {
            OS::get_singleton()->delay_usec(microseconds);
            return microseconds;
        }

        static int vfs_currentTime(sqlite3_vfs* pVfs, double* out)
        {
            // sqlite3_int64 i = 0;
            // const int rc = vfs_currentTimeInt64(0, &i);
            // *out = i / 86400000.0;
            // return rc;
            return ORIGVFS(pVfs)->xCurrentTime(ORIGVFS(pVfs), out);
        }

        static int vfs_getLastError(sqlite3_vfs* pVfs, int n_out, char* out)
        {
            return SQLITE_OK;
        }

        static int vfs_currentTimeInt64(sqlite3_vfs* pVfs, sqlite3_int64* out)
        {
            return ORIGVFS(pVfs)->xCurrentTimeInt64(ORIGVFS(pVfs), out);
        }

        static int file_close(sqlite3_file* file)
        {
            VFSFile* gdfile = (VFSFile*) file;
            gdfile->file_->close();
            gdfile->file_ = nullptr;
            return SQLITE_OK;
        }

        static int file_read(sqlite3_file* file, void* buffer, int amount, sqlite3_int64 offset)
        {
            VFSFile* gdfile = (VFSFile*) file;
            if (!gdfile->file_->is_open()) return SQLITE_IOERR_CLOSE;

            gdfile->file_->seek(offset);
            if (gdfile->file_->get_buffer((uint8_t*) buffer, amount) != amount)
            {
                return SQLITE_IOERR_READ;
            }
            return SQLITE_OK;
        }

        static int file_write(sqlite3_file* file, const void* buffer, int amount, sqlite3_int64 offset)
        {
            VFSFile* gdfile = (VFSFile*) file;
            if (!gdfile->file_->is_open()) return SQLITE_IOERR_CLOSE;

            const uint64_t amount64 = amount;
            gdfile->file_->seek(offset);
            gdfile->file_->store_buffer((const uint8_t*) buffer, amount64);
            const uint64_t written = gdfile->file_->get_position() - offset;
            return written == amount64 ? SQLITE_OK : SQLITE_IOERR_WRITE;
        }

        static int file_truncate(sqlite3_file* file, sqlite3_int64 size)
        {
            VFSFile* gdfile = (VFSFile*) file;
            if (!gdfile->file_->is_open()) return SQLITE_IOERR_CLOSE;

            const Error err = gdfile->file_->resize(size);
            return err == OK ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
        }

        static int file_sync(sqlite3_file* file, int flags)
        {
            VFSFile* gdfile = (VFSFile*) file;
            if (!gdfile->file_->is_open()) return SQLITE_IOERR_CLOSE;

            gdfile->file_->flush();
            return SQLITE_OK;
        }

        static int file_fileSize(sqlite3_file* file, sqlite3_int64* out)
        {
            VFSFile* gdfile = (VFSFile*) file;
            if (!gdfile->file_->is_open()) return SQLITE_IOERR_CLOSE;

            *out = gdfile->file_->get_length();
            return SQLITE_OK;
        }

        static int file_lock(sqlite3_file* file, int lock)
        {
            return SQLITE_OK;
        }

        static int file_unlock(sqlite3_file* file, int lock)
        {
            return SQLITE_OK;
        }

        static int file_checkReservedLock(sqlite3_file* file, int* out)
        {
            *out = 0;
            return SQLITE_OK;
        }

        static int file_fileControl(sqlite3_file* file, int op, void* arg)
        {
            return SQLITE_NOTFOUND;
        }

        static int file_sectorSize(sqlite3_file* file)
        {
            // SQLITE_DEFAULT_SECTOR_SIZE
            return 4096;
        }

        static int file_deviceCharacteristics(sqlite3_file* file)
        {
            return 0;
        }
    };
}

Ref<SQLiteDatabase> SQLiteDatabase::open(const String& p_path, bool p_readonly)
{
    static VFSInitializer vfs;
    const CharString path_u8 = p_path.utf8();
    const char* path_ps = path_u8.get_data();

    Ref<SQLiteDatabase> db = memnew(SQLiteDatabase);
    const int rc = sqlite3_open_v2(path_ps, &db->db_,
        p_readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        GDSQLITE_VFS_NAME);
    ERR_FAIL_COND_V_MSG(rc != SQLITE_OK, Ref<SQLiteDatabase>(), String::utf8(sqlite3_errmsg(db->db_)));
    return db;
}

void SQLiteDatabase::close()
{
    if (db_)
    {
        const int rc = sqlite3_close_v2(db_);
        db_ = nullptr;

        ERR_FAIL_COND_MSG(rc != SQLITE_OK, String::utf8(sqlite3_errmsg(db_)));
    }
}

SQLiteDatabase::~SQLiteDatabase()
{
    close();
}

Ref<SQLiteStatement> SQLiteDatabase::prepare(const String& p_query)
{
    Ref statement = memnew(SQLiteStatement);
    statement->db_ = Ref(this);
    const Error err = statement->prepare(p_query);
    ERR_FAIL_COND_V_MSG(err != OK, Ref<SQLiteStatement>(), String::utf8(sqlite3_errmsg(db_)));
    return statement;
}

Error SQLiteDatabase::exec(const String& p_query)
{
    char* err_msg = nullptr;
    const CharString u8 = p_query.utf8();
    const int rc = sqlite3_exec(db_, u8.get_data(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK)
    {
        const String msg = err_msg ? err_msg : sqlite3_errmsg(db_);
        sqlite3_free(err_msg);
        ERR_FAIL_V_MSG(FAILED, msg);
    }
    return OK;
}
