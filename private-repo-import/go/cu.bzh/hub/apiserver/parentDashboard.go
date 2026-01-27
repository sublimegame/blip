package main

import (
	"cu.bzh/hub/scyllaclient"
)

// Code specific to the Parent Dashboards

func checkParentDashboardIDAndSecretKey(dashboardID, secretKey string) (*scyllaclient.UniverseParentDashboardRecord, error) {
	if dashboardID == "" || secretKey == "" {
		return nil, nil
	}

	dashboard, err := scyllaClientUniverse.GetUniverseParentDashboardByDashboardID(dashboardID)
	if err != nil {
		return nil, err
	}

	// Compute secret key hash
	secretKeyIsValid := scyllaclient.CheckSecretKey(secretKey, dashboard.SecretKeyHash)

	if dashboard == nil || secretKeyIsValid == false {
		// not found or secret key is not valid
		return nil, nil
	}

	// found and valid
	return dashboard, nil
}
