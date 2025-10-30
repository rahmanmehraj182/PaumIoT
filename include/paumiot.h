/**
 * @file paumiot.h
 * @brief Main header file for PaumIoT middleware
 * @author PaumIoT Project
 * @version 1.0.0
 * 
 * This is the primary include file for the PaumIoT IoT Protocol Middleware.
 * It provides the main API and includes all necessary layer headers.
 */

#ifndef PAUMIOT_H
#define PAUMIOT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Standard library includes */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* Project version */
#define PAUMIOT_VERSION_MAJOR 1
#define PAUMIOT_VERSION_MINOR 0
#define PAUMIOT_VERSION_PATCH 0
#define PAUMIOT_VERSION_STRING "1.0.0"

/* Common types and definitions */
#include "common/types.h"
#include "common/errors.h"
#include "common/config.h"
#include "common/logging.h"

/* Layer includes */
#include "smp/smp.h"       /* State Management Plane */
#include "pdl/pdl.h"       /* Protocol Demultiplexing Layer */
#include "pal/pal.h"       /* Protocol Adaptation Layer */
#include "bll/bll.h"       /* Business Logic Layer */
#include "dal/dal.h"       /* Data Acquisition Layer */

/**
 * @brief Main PaumIoT middleware context structure
 */
typedef struct {
    /* Layer contexts */
    smp_context_t *smp;    /**< State Management Plane context */
    pdl_context_t *pdl;    /**< Protocol Demultiplexing Layer context */
    pal_context_t *pal;    /**< Protocol Adaptation Layer context */
    bll_context_t *bll;    /**< Business Logic Layer context */
    dal_context_t *dal;    /**< Data Acquisition Layer context */
    
    /* System state */
    bool initialized;      /**< Initialization status */
    bool running;          /**< Runtime status */
    
    /* Configuration */
    paumiot_config_t *config;  /**< System configuration */
} paumiot_context_t;

/**
 * @brief Initialize the PaumIoT middleware
 * 
 * @param config Configuration parameters
 * @return Middleware context on success, NULL on failure
 */
paumiot_context_t *paumiot_init(const paumiot_config_t *config);

/**
 * @brief Start the PaumIoT middleware
 * 
 * @param ctx Middleware context
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t paumiot_start(paumiot_context_t *ctx);

/**
 * @brief Stop the PaumIoT middleware
 * 
 * @param ctx Middleware context
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t paumiot_stop(paumiot_context_t *ctx);

/**
 * @brief Cleanup and destroy the PaumIoT middleware
 * 
 * @param ctx Middleware context
 */
void paumiot_cleanup(paumiot_context_t *ctx);

/**
 * @brief Get middleware version information
 * 
 * @param major Major version number
 * @param minor Minor version number  
 * @param patch Patch version number
 */
void paumiot_get_version(int *major, int *minor, int *patch);

/**
 * @brief Get middleware version string
 * 
 * @return Version string
 */
const char *paumiot_get_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_H */