#!/bin/sh


if [ -z "$1" ];then
   if [ -z $TSF4G_INSTALL_HOME ];then
	TSF4G_INSTALL_HOME=/usr/local/tsf4g_release/
	export TSF4G_INSTALL_HOME
   fi
else
  TSF4G_INSTALL_HOME=$1
  export TSF4G_INSTALL_HOME
fi  
TSF4G_DIR=./

installAll()
{
	mkdir -p $TSF4G_INSTALL_HOME
	cp -P -rf * $TSF4G_INSTALL_HOME
	find ${TACC_HOME} -name "*.d" -exec rm -rf {} \;>/dev/null 2>&1	
	find ${TACC_HOME} -name "*.o" -exec rm -rf {} \;>/dev/null 2>&1	
}

installAll


tsf4g_home_var_in_profile=`cat ${HOME}/.profile|grep TSF4G_INSTALL_HOME=`
if [ "${tsf4g_home_var_in_profile}" = "" ]; then
    echo "TSF4G_INSTALL_HOME=${TSF4G_INSTALL_HOME}" >>${HOME}/.profile
else
	sed -i  -e "s:^\(TSF4G_INSTALL_HOME=\)[0-9a-zA-Z_/]*:\1${TSF4G_INSTALL_HOME}:g" ${HOME}/.profile
fi

tsf4g_home_var_export_in_profile=`cat ${HOME}/.profile|grep "export TSF4G_INSTALL_HOME"`
if [ "${tsf4g_home_var_export_in_profile}" = "" ]; then
    echo "export TSF4G_INSTALL_HOME" >>${HOME}/.profile
fi

. ~/.profile

echo "Install tsf4g base done"
