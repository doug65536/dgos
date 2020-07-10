#pragma once
#include "dev_storage.h"
#include "cxxstring.h"
#include "hash.h"
struct dev_fs_t;

class dev_fs_file_t : public fs_file_info_t {
public:
    virtual ssize_t read(char *buf, size_t size, off_t offset) = 0;
    virtual ssize_t write(char const *buf, size_t size, off_t offset) = 0;

    // fs_file_info_t interface
    ino_t get_inode() const override = 0;
};

// A registration object which creates a dev_fs_file_t object
// when a device file is opened
class dev_fs_file_reg_t {
public:
    static size_t hash(ext::string const& s)
    {
        return hash_32(s.data(), s.length());
    }

    static size_t hash(char const *s)
    {
        return hash_32(s, strlen(s));
    }

    dev_fs_file_reg_t(ext::string const& name)
        : name(name)
        , name_hash(hash(name))
    {
    }

    virtual ~dev_fs_file_reg_t() = 0;

    virtual dev_fs_file_t *open(int flags, mode_t mode) = 0;

    ext::string name;
    size_t name_hash;
};

dev_fs_t *devfs_create();
void devfs_delete(dev_fs_t *dev_fs);
void devfs_register(dev_fs_file_reg_t *reg);

struct device_t
{
    ext::string name;
    fs_base_t *fs;
};

#include "basic_set.h"

struct dev_fs_t final
        : public fs_base_t
{
    constexpr dev_fs_t() = default;

    FS_BASE_RW_IMPL

    struct device_factory_t {
        virtual ~device_factory_t() = 0;
        virtual fs_file_info_t *open(fs_cpath_t path,
                                     int flags, mode_t mode) = 0;
    };

    struct device_t {
        ext::string name;
        device_factory_t *factory;
    };

    struct device_less_t {
        bool operator()(device_t const& lhs, device_t const& rhs) const
        {
            return lhs.name < rhs.name;
        }
    };

    std::set<device_t, device_less_t> devices;

    struct file_handle_t : public fs_file_info_t {
        enum type_t {
            NODE,
            DIR
        };

        file_handle_t(type_t type)
            : type(type)
        {
        }

        type_t type;
    };

    struct node_handle_t : public file_handle_t {
        node_handle_t()
            : file_handle_t(NODE)
        {
        }
    };

    struct dir_handle_t : public file_handle_t {
        dir_handle_t()
            : file_handle_t(DIR)
        {
        }
    };

    bool register_file(dev_fs_file_reg_t *reg)
    {
        return files.emplace_back(reg);
    }

    std::vector<dev_fs_file_reg_t*> files;
};
