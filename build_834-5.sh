#!/bin/bash
set -e
#***************************************************************************
#Name : build_834_5.sh
#Description : This is the master script to build openwrt image for 834_5
#with opensync
#***************************************************************************
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt
OPTION=${1}
VENDOR_ADTRAN_BRANCH="dev"
VENDOR_ADTRAN_REPO="https://bitbucket.org/smartrg/device-vendor-adtran.git"
PY_VERSION=""
REQ_PY_VER="3.6"
OVERRIDE_PYTHON_VER="3.8"
CUR_PY_VER=$(python3 --version | awk '{print $2}' | cut -c1-3)

#***************************************************************************
#Usage(): This function describe the usage of build_834_5.sh
#***************************************************************************
Usage()
{
   echo ""
   echo "build_834_5.sh : This is the master script to build openwrt image for 834_5 with opensync"
   echo "Syntax: ./build_834_5.sh [build|rebuild|help]"
   echo ""
   echo "Prerequisite:"
   echo "-- The python version should be 3.6 or greater to avoid python script failures"
   echo "-- After cloning of wlan-ap repo execute git credential cache command, to cache GIT credentials"
   echo "   git config credential.helper 'cache --timeout=1800'"
   echo ""
   echo "options:"
   echo "build    setup, generate config, patch files and builds the image for SR400ac"
   echo "rebuild  re-compile and build 834_5 image"
   echo "help     help list"
   echo ""
   exit 1
}
#***************************************************************************
#isPythonVerValid(): This function is to validate the required python version
#***************************************************************************
isPythonVerValid()
{
   REQ_MJR_PY_VER=$(echo $REQ_PY_VER | cut -d'.' -f1)
   REQ_MIN_PY_VER=$(echo $REQ_PY_VER | cut -d'.' -f2)
   CUR_MJR_PY_VER=$(echo $CUR_PY_VER | cut -d'.' -f1)
   CUR_MIN_PY_VER=$(echo $CUR_PY_VER | cut -d'.' -f2)

   if [ -f "/usr/bin/python$OVERRIDE_PYTHON_VER" ]; then
      PY_VERSION=$OVERRIDE_PYTHON_VER
      echo "Valid python $PY_VERSION version exist. Continue the build process"
      return 0
   else
      if [ "$CUR_MJR_PY_VER" -ge "$REQ_MJR_PY_VER" ]; then
         if [ "$CUR_MIN_PY_VER" -ge "$REQ_MIN_PY_VER" ]; then
            echo "Valid python $PY_VERSION version exist. Continue the build process"
            PY_VERSION=$CUR_PY_VER
            return 0
         fi
      fi
   fi
   return 1
}

#***************************************************************************
#cloneVendorAdtranRepo(): This function is to clone device-vendor-adtran repo
#***************************************************************************
cloneVendorAdtranRepo()
{
   cd $ROOT_PATH
   cd ..
   VENDOR_ADTRAN_PATH="${PWD}/device-vendor-adtran"
   if [ ! -d "$VENDOR_ADTRAN_PATH" ]; then
      git clone -b $VENDOR_ADTRAN_BRANCH $VENDOR_ADTRAN_REPO
   else
      echo "device-vendor-adtran repo exists.Skip clone of vendor-adtran repo."
   fi
   cd $ROOT_PATH
}

#***************************************************************************
#setup(): This function is to trigger setup.py to clone openwrt
#***************************************************************************
setup()
{
   echo "### Trigger setup.py"
   if [ ! "$(ls -A $BUILD_DIR)" ]; then
       python$PY_VERSION setup.py --setup || exit 1
   else
       python$PY_VERSION setup.py --rebase
       echo "### OpenWrt repo already setup"
   fi
}

#***************************************************************************
#genConfig(): This function is to genrate openwrt's .config file based on
#the inputs provided
#***************************************************************************
genConfig()
{
   echo "### generate .config for 834_5 target ..."
   cd $BUILD_DIR
   python$PY_VERSION $BUILD_DIR/scripts/gen_config.py 834-5 wlan-ap-consumer wifi-834-5 || exit 1
   cd ..
}

#***************************************************************************
#applyOpenSyncMakefilePatch(): This function is to patch opensync's Makefile
# to update vendor repo vendor-plume-openwrt to device-vendor-adtran repo.
#***************************************************************************
applyOpenSyncMakefilePatch()
{
    echo "### Apply opensync Makefile patches to get device-vendor-adtran repo"
    cd $ROOT_PATH
    cd ..
    VENDOR_ADTRAN_PATH="${PWD}/device-vendor-adtran"
    if [ ! -d "$VENDOR_ADTRAN_PATH" ]; then
       echo "device-vendor-adtran directory not found!!"
       exit 1
    fi
    cd $ROOT_PATH
    CFG80211_SEARCH_STR="git@github.com:plume-design/opensync-platform-cfg80211.git"
    CFG80211_REPLACE_STR="https://github.com/plume-design/opensync-platform-cfg80211.git"
    VENDOR_SEARCH_STR=".*git@github.com:plume-design/opensync-vendor-plume-openwrt.git.*"
    VENDOR_REPLACE_STR="\tgit clone --single-branch --branch $VENDOR_ADTRAN_BRANCH file://$VENDOR_ADTRAN_PATH \$(PKG_BUILD_DIR)/vendor/adtran"
    OPENSYNC_MAKEFILE_PATH="$ROOT_PATH/feeds/wlan-ap-consumer/opensync/Makefile"

    sed -i "s#$CFG80211_SEARCH_STR#$CFG80211_REPLACE_STR#" $OPENSYNC_MAKEFILE_PATH
    sed -i "s#$VENDOR_SEARCH_STR#$VENDOR_REPLACE_STR#" $OPENSYNC_MAKEFILE_PATH
}

#***************************************************************************
#addServiceProviderCerts(): This function is to untar service-provider_opensync-dev.tgz
#from device-vendor-adtran to feeds/wlan-ap-consumer/opensync
#***************************************************************************
addServiceProviderCerts()
{
    echo "### Add serive provider certificates"
    cd $ROOT_PATH
    if [ ! -f "../device-vendor-adtran/third-party/target/SR400ac/service-provider_opensync-dev.tgz" ]; then
       echo "../device-vendor-adtran/third-party/target/SR400ac/service-provider_opensync-dev.tgz tar file not found. exit build!!"
       exit 1
    fi
    tar -xzvf ../device-vendor-adtran/third-party/target/SR400ac/service-provider_opensync-dev.tgz -C $ROOT_PATH/feeds/wlan-ap-consumer/opensync/src/service-provider
}

#***************************************************************************
#applySetupConfigPatch(): This function is to patch setup.py 
# to update config.yml to openwrt21.2 new config file 
#***************************************************************************
applySetupConfigPatch()
{
    echo "### Apply setup patch to get openwrt 21.02 version "
    cd $ROOT_PATH

    OWRT21_CONFIG_PATH="$ROOT_PATH/configOwrt21_02.yml"
    if [ ! -f "$OWRT21_CONFIG_PATH" ]; then
       echo "openwrt 21.02 config file  not found!!"
       exit 1
    fi
    cd $ROOT_PATH
    CONFIG_SEARCH_STR="config.yml"
    CONFIG_REPLACE_STR="configOwrt21_02.yml"
    OSYNC_SETUP_PATH="$ROOT_PATH/setup.py"

    sed -i "s#$CONFIG_SEARCH_STR#$CONFIG_REPLACE_STR#" $OSYNC_SETUP_PATH

    # START - patch git command for specific commit ID
    # This will we used until we use banana pi r64 build. 
    # Once moved to openwrt_21.02 modifying git commands in setup.py will be removed    
    OWRT_REVISION=$(cat $ROOT_PATH/configOwrt21_02.yml | grep revision | awk '{print $2}')
    BRANCH_CHKOUT_SEARCH_STR="checkout\", config\[\"branch\"\]"
    BRANCH_CHKOUT_REPLACE_STR="checkout\", \"-b\", config\[\"branch\"\]"
    RESET_HEAD_SEARCH_STR="\[\"git\", \"reset\", \"--hard\", config.get(\"revision\", config\[\"branch\"\])\]"
    RESET_HEAD_REPLACE_STR="\[\"git\", \"reset\", \"--hard\", config\[\"revision\"\]\]"
    sed -i "s#$BRANCH_CHKOUT_SEARCH_STR#$BRANCH_CHKOUT_REPLACE_STR#" $OSYNC_SETUP_PATH 
    sed -i "s#$RESET_HEAD_SEARCH_STR#$RESET_HEAD_REPLACE_STR#" $OSYNC_SETUP_PATH
    #END - patch git command for specific commit ID
}

#***************************************************************************
#applyPatches(): This function is to copy patches from device-vendor-adtran
# to wlan-ap-consumer and apply patches
#***************************************************************************
applyPatches()
{
    echo "### Copy patches from device-vendor-adtran to wlan-ap-consumer"
    cd $ROOT_PATH
    if [ ! -d "../device-vendor-adtran/additional-patches/target/834-5/patches/openwrt/" ]; then
       echo "device-vendor-adtan directory doesn't have openwrt patches. exit!!"
#       exit 1
    fi

    if [ ! -d "../device-vendor-adtran/additional-patches/target/834-5/patches/opensync/" ]; then
       echo "device-vendor-adtan directory doesn't have opensync patches. exit!!"
       exit 1
    fi

    cp ../device-vendor-adtran/additional-patches/target/834-5/patches/opensync/* $ROOT_PATH/feeds/wlan-ap-consumer/opensync/patches/.
#    cp -r ../device-vendor-adtran/additional-patches/target/834-5/patches/openwrt/* $ROOT_PATH/feeds/wlan-ap-consumer/additional-patches/patches/openwrt/.
    /bin/sh $ROOT_PATH/feeds/wlan-ap-consumer/additional-patches/apply-patches.sh
}

#***************************************************************************
#applyOpenwrtPatches(): This function is to add openwrt patches specific for
#834-5
#***************************************************************************
applyOpenwrtPatches()
{
    TARGET_SPECFIC_OWRT_PATCH_PATH="../../device-vendor-adtran/additional-patches/target/834-5/patches/owrtPatches"
    cd $BUILD_DIR
    for file in "$TARGET_SPECFIC_OWRT_PATCH_PATH"/*
    do
        echo "patch -d "$BUILD_DIR" -p1 < $file"
        patch -d "$BUILD_DIR" -p1 < "$file"
    done
    cd $ROOT_PATH
}

#***************************************************************************
#addPkgFeeds(): This function is to add required packages as feeds for
#834-5
#***************************************************************************

addPkgFeeds()
{
    TARGET_SPECFIC_FEEDS_PATH="../../device-vendor-adtran/additional-patches/target/834-5/feeds"
    PKG_FEEDS_PATH="$BUILD_DIR/feeds/packages/libs"
    PKG_NET_CONF_PATH="$BUILD_DIR/package/network/config"
    WLAN_AP_CONSUMER_PATH="$ROOT_PATH/feeds/wlan-ap-consumer"
    echo "### Copy packages in feeds from device-vendor-adtran to wlan-ap-consumer and libs path"
    cd $BUILD_DIR

    # update wlan-ap-consumer with required packages
    rm -rf $WLAN_AP_CONSUMER_PATH/miniupnpd
    rm -rf $WLAN_AP_CONSUMER_PATH/python3-jinja2
    rm -rf $WLAN_AP_CONSUMER_PATH/python3-kconfiglib
    rm -rf $WLAN_AP_CONSUMER_PATH/python3-markupsafe
    cp -r $TARGET_SPECFIC_FEEDS_PATH/wlan-ap-consumer/* $WLAN_AP_CONSUMER_PATH/.

    # update protobuf with required version of package
    rm -rf $PKG_FEEDS_PATH/protobuf/*
    rm -rf $PKG_FEEDS_PATH/protobuf-c/*
    cp -r $TARGET_SPECFIC_FEEDS_PATH/pkg-overrides/protobuf/* $PKG_FEEDS_PATH/protobuf/.
    cp -r $TARGET_SPECFIC_FEEDS_PATH/pkg-overrides/protobuf-c/* $PKG_FEEDS_PATH/protobuf-c/.

    # update netifd with older version 
    rm -rf $PKG_NET_CONF_PATH/netifd/*
    cp -r $TARGET_SPECFIC_FEEDS_PATH/pkg-overrides/netifd/* $PKG_NET_CONF_PATH/netifd/.

    # Add libcap package
    cp -r $TARGET_SPECFIC_FEEDS_PATH/pkg-overrides/libcap $PKG_FEEDS_PATH/.
    ln -s ../../../feeds/packages/libs/libcap ./package/feeds/packages/libcap
 
    cd $ROOT_PATH
}
#***************************************************************************
#compileOpenwrt(): This function is to make openwrt image
#***************************************************************************
compileOpenwrt()
{
    echo "### Building image ..."
    cd $BUILD_DIR
    if [ ! -d "$BUILD_DIR" ]; then
       echo "$BUILD_DIR Not found exit compilation!!"
       exit 1
    fi
    make -j8 V=s 2>&1 | tee build.log
    cd $ROOT_PATH
    echo "Done"
}

#***************************************************************************
#triggerBuild(): This function is the main build function to clone VndrAdtran,
# apply patches, add serviceprovider certs, setup, generate owrt .config
# and compile the image
#***************************************************************************
triggerBuild()
{
   isPythonVerValid
   chkPyVer=$?
   if [ $chkPyVer != 0 ]; then
      echo "Invalid python version. Halt build process"
      exit 1
   fi

   cd $ROOT_PATH
   cloneVendorAdtranRepo
   applyOpenSyncMakefilePatch
   addServiceProviderCerts
   applySetupConfigPatch
   setup
   applyOpenwrtPatches
   genConfig
   addPkgFeeds
   applyPatches
   compileOpenwrt
}

#***************************************************************************
#main() : This is the main function of this script
#***************************************************************************
main()
{
   if [ "${OPTION}" == "build" ]; then
      triggerBuild;
   elif [ "${OPTION}" == "rebuild" ]; then
      compileOpenwrt;
   elif [ "${OPTION}" == "help" ]; then
      Usage;
   else
      Usage;
   fi
}

if [ -z "$1" ]; then
   Usage
   exit 1
fi

main
exit 0
