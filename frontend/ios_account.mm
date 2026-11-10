// PokeHub's account on an iPhone or an iPad (what Account.java does on Android): signing in with
// Google through PokeHub's own server (SSR_ACCOUNT_URL, a Cloudflare Worker).
//  - Sign In opens iOS's own sign-in sheet (ASWebAuthenticationSession) at the server's
//    /auth/start, with a random state kept here and platform=ios;
//  - the server, once Google has answered, sends the sheet to pokehub://signin?token=...&state=...
//    &email=...&name=..., which the sheet catches (no URL scheme of the app's own is needed);
//  - the state must be the one kept, then the session (its token, email and name) is kept in the
//    app's own preferences. Sign Out forgets it (and tells the server, which ends the session).
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <AuthenticationServices/AuthenticationServices.h>
#import <Security/SecRandom.h>
#include <string>

@interface PokeHubAuthAnchor : NSObject <ASWebAuthenticationPresentationContextProviding>
@end
@implementation PokeHubAuthAnchor
- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession*)session
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class])
            for (UIWindow* w in ((UIWindowScene*)scene).windows)
                if (w.isKeyWindow) return w;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class] && ((UIWindowScene*)scene).windows.count) return ((UIWindowScene*)scene).windows.firstObject;
    return [[UIWindow alloc] init];
}
@end

static ASWebAuthenticationSession* gAuth;
static PokeHubAuthAnchor* gAnchor;
static NSUserDefaults* Prefs() { return NSUserDefaults.standardUserDefaults; }
static NSString* Str(NSString* key) { NSString* s = [Prefs() stringForKey:key]; return s ? s : @""; }

void IosAccountSignIn(const char* server)
{
    if (!server || !*server) return;
    uint8_t b[24];
    if (SecRandomCopyBytes(kSecRandomDefault, sizeof b, b) != errSecSuccess) return;
    NSMutableString* state = [NSMutableString string];
    for (uint8_t x : b) [state appendFormat:@"%02x", x];
    NSURLComponents* start = [NSURLComponents componentsWithString:[NSString stringWithFormat:@"%s/auth/start", server]];
    start.queryItems = @[ [NSURLQueryItem queryItemWithName:@"state" value:state], [NSURLQueryItem queryItemWithName:@"platform" value:@"ios"] ];
    NSURL* url = start.URL;
    NSString* want = [state copy];
    dispatch_async(dispatch_get_main_queue(), ^{
        gAuth = [[ASWebAuthenticationSession alloc] initWithURL:url callbackURLScheme:@"pokehub" completionHandler:^(NSURL* back, NSError* error) {
            if (!back) { NSLog(@"[account] sign-in sheet closed: %@", error); return; }
            NSURLComponents* r = [NSURLComponents componentsWithURL:back resolvingAgainstBaseURL:NO];
            NSString *got = nil, *token = nil, *email = @"", *name = @"";
            for (NSURLQueryItem* q in r.queryItems)
            {
                if ([q.name isEqualToString:@"state"]) got = q.value;
                else if ([q.name isEqualToString:@"token"]) token = q.value;
                else if ([q.name isEqualToString:@"email"] && q.value) email = q.value;
                else if ([q.name isEqualToString:@"name"] && q.value) name = q.value;
            }
            if (![got isEqualToString:want] || token.length < 32) { NSLog(@"[account] sign-in link not ours, ignored"); return; }
            [Prefs() setObject:token forKey:@"pokehub_token"];
            [Prefs() setObject:email forKey:@"pokehub_email"];
            [Prefs() setObject:name forKey:@"pokehub_name"];
        }];
        if (!gAnchor) gAnchor = [PokeHubAuthAnchor new];
        gAuth.presentationContextProvider = gAnchor;
        if (![gAuth start]) NSLog(@"[account] the sign-in sheet did not open");
    });
}

// "email\tname" when signed in, else "".
std::string IosAccountInfo()
{
    if (!Str(@"pokehub_token").length) return "";
    return std::string(Str(@"pokehub_email").UTF8String) + "\t" + Str(@"pokehub_name").UTF8String;
}

// "server\ttoken" for the cloud saves and the trades, as Android's accountSession.
std::string IosAccountSession(const char* server)
{
    NSString* t = Str(@"pokehub_token");
    if (!t.length || !server || !*server) return "";
    return std::string(server) + "\t" + t.UTF8String;
}

void IosAccountSignOut(const char* server)
{
    NSString* token = Str(@"pokehub_token");
    [Prefs() removeObjectForKey:@"pokehub_token"];
    [Prefs() removeObjectForKey:@"pokehub_email"];
    [Prefs() removeObjectForKey:@"pokehub_name"];
    if (!token.length || !server || !*server) return;
    // (The server ends the session too; if it cannot be reached, the session just expires there.)
    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"%s/auth/signout", server]]];
    req.HTTPMethod = @"POST";
    [req setValue:[@"Bearer " stringByAppendingString:token] forHTTPHeaderField:@"Authorization"];
    req.timeoutInterval = 10;
    [[NSURLSession.sharedSession dataTaskWithRequest:req] resume];
}
