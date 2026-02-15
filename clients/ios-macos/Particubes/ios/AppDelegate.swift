//
//  AppDelegate.swift
//  ios
//
//  Created by Adrien Duermael on 2/13/20.
//

import UIKit
import Firebase

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {

    /// push token type for Apple platformes
    private let pushTokenTypeApple = "apple"

    // for devices with iOS < 13.0
    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?,
                     continue userActivity: NSUserActivity,
                     restorationHandler: @escaping ([Any]?) -> Void) -> Bool {
        // this function is not called on iOS 13 and later

        vxapplication_init()

        FirebaseApp.configure()

        return false
    }

    // MARK: UISceneSession Lifecycle

    @available(iOS 13.0, *)
    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        // Called when a new scene session is being created.
        // Use this method to select a configuration to create the new scene with.
        return UISceneConfiguration(name: "Default Configuration", sessionRole: connectingSceneSession.role)
    }

    @available(iOS 13.0, *)
    func application(_ application: UIApplication, didDiscardSceneSessions sceneSessions: Set<UISceneSession>) {
        // Called when the user discards a scene session.
        // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
        // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
    }
    
    // for devices with iOS < 13.0
    func applicationWillEnterForeground(_ application: UIApplication) {
        vxapplication_willEnterForeground();
    }
    
    // for devices with iOS < 13.0
    func applicationDidEnterBackground(_ application: UIApplication) {
        vxapplication_didEnterBackground();
    }
    
    // for devices with iOS < 13.0
    func applicationDidBecomeActive(_ application: UIApplication) {
        UIApplication.shared.isIdleTimerDisabled = true
        vxapplication_didBecomeActive()
    }
    
    // for devices with iOS < 13.0
    func applicationWillResignActive(_ application: UIApplication) {
        UIApplication.shared.isIdleTimerDisabled = false
        vxapplication_willResignActive()
    }

    func application(_ application: UIApplication, didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data) {
        let tokenParts = deviceToken.map { data in String(format: "%02.2hhx", data) }
        let token = tokenParts.joined()
        print("📣[PushNotifs] didRegisterForRemoteNotificationsWithDeviceToken: \(token)")
        // notify VXApplication
        vxapplication_didRegisterForRemoteNotifications(pushTokenTypeApple, token);
    }

    func application( _ application: UIApplication, didFailToRegisterForRemoteNotificationsWithError error: Error) {
        // token couldn't be obtained
        // TODO: retry?
    }

    func application(_ application: UIApplication,
                     didReceiveRemoteNotification userInfo: [AnyHashable : Any],
                     fetchCompletionHandler completionHandler: @escaping (UIBackgroundFetchResult) -> Void) {
        var title = ""
        var body = ""
        var category = ""
        var badge = 0

        // Access the 'aps' dictionary, which contains standard push notification fields
        if let aps = userInfo["aps"] as? [String: Any] {
            // Extract alert information
            if let alert = aps["alert"] as? [String: Any] {
                title = alert["title"] as? String ?? "No Title"
                body = alert["body"] as? String ?? "No Body"
            }

            // Extract badge number
            if let badgeCount = aps["badge"] as? Int {
                badge = badgeCount
            }
        }

        // Access custom data fields
        if let c = userInfo["category"] as? String {
            category = c
        }

        DispatchQueue.main.async {
            // set badge count, not set otherwise when app is in foreground
            UIApplication.shared.applicationIconBadgeNumber = badge
            vxapplication_didReceiveNotification(title.cString(using: .utf8), body.cString(using: .utf8), category, UInt32(badge));
        }
    }

    func application(_ application: UIApplication, supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {

        let orientation = String(cString: device_screen_allowed_orientation())

        switch orientation {
        case "portrait":
            return .portrait
        case "landscape":
            return .landscape
        default:
            return .all
        }
    }
}
