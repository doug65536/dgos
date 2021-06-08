#include "kmodule.h"
#include "dev_storage.h"
#include "printk.h"
#include "mm.h"
#include "bitsearch.h"

__BEGIN_ANONYMOUS

class ntfs_fs_t final : public fs_base_t {

    friend class ntfs_factory_t;

    struct file_handle_t : public fs_file_info_t {
        file_handle_t() = default;
        virtual ~file_handle_t() = default;

        ino_t get_inode() const override
        {
            return inode;
        }

        ino_t inode;
    };

    FS_BASE_RW_IMPL
};

class ntfs_factory_t : public fs_factory_t {
public:
    ntfs_factory_t();

    _use_result
    fs_base_t *mount(fs_init_info_t *conn) override;
};

ntfs_factory_t::ntfs_factory_t()
    : fs_factory_t("ntfs")
{
}

fs_base_t *ntfs_factory_t::mount(fs_init_info_t *conn)
{
    return nullptr;
}

ntfs_factory_t ntfs_factory();

__END_ANONYMOUS
