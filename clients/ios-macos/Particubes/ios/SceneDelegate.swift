//
//  SceneDelegate.swift
//  ios
//
//  Created by Adrien Duermael on 2/13/20.
//

import UIKit
import Firebase

class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    @available(iOS 13.0, *)
    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {

        vxapplication_init()

        FirebaseApp.configure()

        // Use this method to optionally configure and attach the UIWindow `window` to the provided UIWindowScene `scene`.
        // If using a storyboard, the `window` property will automatically be initialized and attached to the scene.
        // This delegate does not imply the connecting scene or session are new (see `application:configurationForConnectingSceneSession` instead).
        guard let _ = (scene as? UIWindowScene) else { return }

        // This function is executed when a Universal Link is opened if the app isn't currently running (foreground nor background)

        // Handle Universal Links : get URL from the incoming user activity.
        if let userActivity = connectionOptions.userActivities.first,
           userActivity.activityType == NSUserActivityTypeBrowsingWeb,
           let incomingURL = userActivity.webpageURL {
            vxapplication_openURL(incomingURL.absoluteString)
        }
    }

    @available(iOS 13.0, *)
    func scene(_ scene: UIScene, continue userActivity: NSUserActivity) {
        // This function is executed with a Universal Link is opened and the app is currently running (foreground or background)

        // Handle Universal Links : get URL from the incoming user activity.
        if userActivity.activityType == NSUserActivityTypeBrowsingWeb,
           let incomingURL = userActivity.webpageURL {
            vxapplication_openURL(incomingURL.absoluteString)
        }
    }

    @available(iOS 13.0, *)
    func sceneDidDisconnect(_ scene: UIScene) {
        // Called as the scene is being released by the system.
        // This occurs shortly after the scene enters the background, or when its session is discarded.
        // Release any resources associated with this scene that can be re-created the next time the scene connects.
        // The scene may re-connect later, as its session was not neccessarily discarded (see `application:didDiscardSceneSessions` instead).
    }

    @available(iOS 13.0, *)
    func sceneDidBecomeActive(_ scene: UIScene) {
        // Called when the scene has moved from an inactive state to an active state.
        // Use this method to restart any tasks that were paused (or not yet started) when the scene was inactive.
        
        UIApplication.shared.isIdleTimerDisabled = true
        vxapplication_didBecomeActive()
    }

    @available(iOS 13.0, *)
    func sceneWillResignActive(_ scene: UIScene) {
        // Called when the scene will move from an active state to an inactive state.
        // This may occur due to temporary interruptions (ex. an incoming phone call).
        
        UIApplication.shared.isIdleTimerDisabled = false
        vxapplication_willResignActive()
    }

    @available(iOS 13.0, *)
    func sceneWillEnterForeground(_ scene: UIScene) {
        // Called as the scene transitions from the background to the foreground.
        // Use this method to undo the changes made on entering the background.

        vxapplication_willEnterForeground()
    }

    @available(iOS 13.0, *)
    func sceneDidEnterBackground(_ scene: UIScene) {
        // Called as the scene transitions from the foreground to the background.
        // Use this method to save data, release shared resources, and store enough scene-specific state information
        // to restore the scene back to its current state.
        
        vxapplication_didEnterBackground()
    }
}

