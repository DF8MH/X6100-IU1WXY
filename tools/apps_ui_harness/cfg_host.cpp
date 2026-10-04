/* A real cfg_sm (default values, no database) so that Settings can render. */
#include "cfg/settings_manager.h"
SettingsManager cfg_sm;

// Cluster spots on the main screen (topbar demo)
#include "cfg/cfg_api.h"
static int cluster_spots_init = (cfg_cluster_spots_wf = &cfg_sm.p_cluster_spots_wf, cfg_cluster_column = &cfg_sm.p_cluster_column, cfg_cluster_order = &cfg_sm.p_cluster_order, cfg_cluster_all_bands = &cfg_sm.p_cluster_all_bands, cfg_band_plan = &cfg_sm.p_band_plan, 0);
// CW trainer
static int cwt_init = (cfg_cwt_level = &cfg_sm.p_cwt_level, cfg_cwt_group = &cfg_sm.p_cwt_group,
                       cfg_key_speed = &cfg_sm.p_key_speed, cfg_key_train = &cfg_sm.p_key_train, 0);
// WSPR
static int wspr_init = (cfg_wspr_tx_pct = &cfg_sm.p_wspr_tx_pct, cfg_wspr_tx_freq = &cfg_sm.p_wspr_tx_freq,
                        cfg_wspr_upload = &cfg_sm.p_wspr_upload, 0);
// Field Ops
static int field_init = (cfg_field_boot = &cfg_sm.p_field_boot, 0);
// SWR scan
static int swr_init = (cfg_swrscan_linear = &cfg_sm.p_swrscan_linear, cfg_swrscan_span = &cfg_sm.p_swrscan_span, 0);
