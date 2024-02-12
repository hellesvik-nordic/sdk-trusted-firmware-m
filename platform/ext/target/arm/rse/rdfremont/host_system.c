/*
* Copyright (c) 2024, Arm Limited. All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*
*/

#include <stdbool.h>
#include <stdint.h>

#include "device_definition.h"
#include "host_base_address.h"
#include "host_system.h"
#include "tfm_hal_device_header.h"
#include "tfm_plat_otp.h"

static struct host_system_t host_system_data = {0};

#if defined(RD_SYSCTRL_NI_TOWER) || defined(RD_PERIPH_NI_TOWER)
/*
 * Initializes the ATU region before configuring the NI-Tower. This function
 * maps the physical base address of the NI-Tower instance received as the
 * parameter to a logical address HOST_NI_TOWER_BASE.
 */
static int ni_tower_pre_init(uint64_t ni_tower_phys_address)
{
    enum atu_error_t atu_err;
    enum atu_roba_t roba_value;

    atu_err = atu_initialize_region(
                &ATU_DEV_S,
                HOST_NI_TOWER_ATU_ID,
                HOST_NI_TOWER_BASE,
                ni_tower_phys_address,
                HOST_NI_TOWER_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    roba_value = ATU_ROBA_SET_1;
    atu_err = set_axnsc(&ATU_DEV_S, roba_value, HOST_NI_TOWER_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    roba_value = ATU_ROBA_SET_0;
    atu_err = set_axprot1(&ATU_DEV_S, roba_value, HOST_NI_TOWER_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    return 0;
}

/* Un-initializes the ATU region after configuring the NI-Tower */
static int ni_tower_post_init(void)
{
    enum atu_error_t atu_err;

    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_NI_TOWER_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    return 0;
}
#endif

#ifdef RD_SYSCTRL_NI_TOWER
/*
 * Programs the System control NI-Tower for nodes under Always-On (AON) domain.
 */
static int ni_tower_sysctrl_aon_init(void)
{
    int err;

    err = ni_tower_pre_init(host_system_data.info.chip_ap_phys_base +
                            HOST_SYSCTRL_NI_TOWER_PHYS_BASE);
    if (err != 0) {
        return err;
    }

    SYSCTRL_NI_TOWER_DEV.chip_addr_offset =
            host_system_data.info.chip_ap_phys_base;
    err = program_sysctrl_ni_tower_aon();
    if (err != 0) {
        return err;
    }

    err = ni_tower_post_init();
    if (err != 0) {
        return err;
    }

    return 0;
}

/*
 * Programs the System control NI-Tower for nodes under SYSTOP domain.
 */
static int ni_tower_sysctrl_systop_init(void)
{
    int err;

    err = ni_tower_pre_init(host_system_data.info.chip_ap_phys_base +
                            HOST_SYSCTRL_NI_TOWER_PHYS_BASE);
    if (err != 0) {
        return err;
    }

    SYSCTRL_NI_TOWER_DEV.chip_addr_offset =
            host_system_data.info.chip_ap_phys_base;
    err = program_sysctrl_ni_tower_systop();
    if (err != 0) {
        return err;
    }

    err = ni_tower_post_init();
    if (err != 0) {
        return err;
    }

    return 0;
}
#endif

#ifdef RD_PERIPH_NI_TOWER
/*
 * Programs the Peripheral NI-Tower.
 */
static int ni_tower_periph_init(void)
{
    int err;

    err = ni_tower_pre_init(host_system_data.info.chip_ap_phys_base +
                            HOST_PERIPH_NI_TOWER_PHYS_BASE);
    if (err != 0) {
        return err;
    }

    PERIPH_NI_TOWER_DEV.chip_addr_offset =
        host_system_data.info.chip_ap_phys_base;
    err = program_periph_ni_tower();
    if (err != 0) {
        return err;
    }

    err = ni_tower_post_init();
    if (err != 0) {
        return err;
    }

    return 0;
}
#endif

#ifdef HOST_SMMU
/*
 * Initialize and bypass Granule Protection Check (GPC) to allow RSE and SCP
 * to access HOST AP peripheral regions.
 */
static int32_t sysctrl_smmu_init(void)
{
    enum atu_roba_t roba_value;
    enum atu_error_t atu_err;
    enum smmu_error_t smmu_err;

    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SYSCTRL_SMMU_ATU_ID,
                                    HOST_SYSCTRL_SMMU_BASE,
                                    (host_system_data.info.chip_ap_phys_base +
                                        HOST_SYSCTRL_SMMU_PHYS_BASE),
                                    HOST_SYSCTRL_SMMU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    roba_value = ATU_ROBA_SET_1;
    atu_err = set_axnsc(&ATU_DEV_S, roba_value, HOST_SYSCTRL_SMMU_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    roba_value = ATU_ROBA_SET_0;
    atu_err = set_axprot1(&ATU_DEV_S, roba_value, HOST_SYSCTRL_SMMU_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }

    /* Disable GPC */
    smmu_err = smmu_gpc_disable(&HOST_SYSCTRL_SMMU_DEV);
    if (smmu_err != SMMU_ERR_NONE){
        return -1;
    }

    /* Allow Access via SMMU */
    smmu_err = smmu_access_enable(&HOST_SYSCTRL_SMMU_DEV);
    if (smmu_err != SMMU_ERR_NONE){
        return -1;
    }

    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SYSCTRL_SMMU_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return -1;
    }
    return 0;
}
#endif

/* Read Chip ID from OTP */
static int read_chip_id(uint32_t *chip_id)
{
    int err;
    uint32_t otp_chip_id;

    err = tfm_plat_otp_read(PLAT_OTP_ID_RSE_ID,
                            sizeof(otp_chip_id),
                            (uint8_t*)&otp_chip_id);
    if (err != 0)
        return err;

    *chip_id = otp_chip_id;
    return 0;
}

/* Initialize host system by collecting fixed data about the host system */
int host_system_init(void)
{
    int res;

    res = read_chip_id(&host_system_data.info.chip_id);
    if (res != 0) {
        host_system_data.info.chip_id = 0;
        return res;
    }
    host_system_data.info.chip_ap_phys_base =
                    HOST_AP_CHIP_N_PHYS_BASE(host_system_data.info.chip_id);

    host_system_data.initialized = true;
    return 0;
}

/* Get info struct containing fixed data about the host system */
int host_system_get_info(struct host_system_info_t **info)
{
    if (info == NULL) {
        return -1;
    }

    if (host_system_data.initialized == false) {
        return -1;
    }

    *info = &host_system_data.info;
    return 0;
}

int host_system_prepare_mscp_access(void)
{
#ifdef RD_SYSCTRL_NI_TOWER
    int res;

    /* Configure System Control NI-Tower for nodes under AON power domain */
    res = ni_tower_sysctrl_aon_init();
    if (res != 0) {
        return res;
    }
#endif
    return 0;
}

int host_system_prepare_ap_access(void)
{
    int res;

    (void)res;

    /*
     * AP cannot be accessed until SCP setup is complete so wait for signal
     * from SCP.
     */
    while (host_system_data.status.scp_systop_ready == false) {
        __WFE();
    }

#ifdef RD_SYSCTRL_NI_TOWER
    /* Configure System Control NI-Tower for nodes under SYSTOP power domain */
    res = ni_tower_sysctrl_systop_init();
    if (res != 0) {
        return 1;
    }
#endif

#ifdef HOST_SMMU
    /* Initialize the SYSCTRL SMMU for boot time */
    res = sysctrl_smmu_init();
    if (res != 0) {
        return 1;
    }
#endif

#ifdef RD_PERIPH_NI_TOWER
    /* Configure Peripheral NI-Tower */
    res = ni_tower_periph_init();
    if (res != 0) {
        return 1;
    }
#endif

    return 0;
}

void host_system_scp_signal_ap_ready(void)
{
    host_system_data.status.scp_systop_ready = true;
}
