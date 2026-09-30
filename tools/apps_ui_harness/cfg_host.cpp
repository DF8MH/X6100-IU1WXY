/* A real cfg_sm (default values, no database) so that Settings can render. */
#include "cfg/settings_manager.h"
SettingsManager cfg_sm;

// Cluster spots on the main screen (topbar demo)
#include "cfg/cfg_api.h"
static int cluster_spots_init = (cfg_cluster_spots_wf = &cfg_sm.p_cluster_spots_wf, 0);
