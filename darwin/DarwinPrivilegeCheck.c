/*
htop - darwin/DarwinPrivilegeCheck.c
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DarwinPrivilegeCheck.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <Security/Authorization.h>

bool DarwinPrivilegeCheck_isRoot(void) {
    return geteuid() == 0;
}

bool DarwinPrivilegeCheck_requestRoot(void) {
    if (DarwinPrivilegeCheck_isRoot()) {
        return true;
    }

    OSStatus status;
    AuthorizationRef authRef;
    AuthorizationRights rights;
    AuthorizationFlags flags;
    
    // 初始化授权引用
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, 
                               kAuthorizationFlagDefaults, &authRef);
    if (status != errAuthorizationSuccess) {
        return false;
    }
    
    // 设置权限和标志
    rights.count = 0;
    rights.items = NULL;
    flags = kAuthorizationFlagDefaults |
            kAuthorizationFlagInteractionAllowed |
            kAuthorizationFlagPreAuthorize |
            kAuthorizationFlagExtendRights;
    
    // 请求提权
    status = AuthorizationCopyRights(authRef, &rights, 
                                    kAuthorizationEmptyEnvironment,
                                    flags, NULL);
    
    // 释放授权引用
    AuthorizationFree(authRef, kAuthorizationFlagDefaults);
    
    return (status == errAuthorizationSuccess);
}