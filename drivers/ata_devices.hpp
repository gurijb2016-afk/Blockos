#pragma once

#include "ata_pio.hpp"

AtaPio& ata_boot_disk();

AtaPio& ata_data_disk();

AtaPio& ata_fs_disk();
