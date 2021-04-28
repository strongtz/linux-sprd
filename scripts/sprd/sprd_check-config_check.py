#!/usr/bin/env python
#-*- coding: utf-8 -*-
#!/usr/bin/python3

import os
import sys
import csv
import re

tmp_path_def = "./tmp_config_check/"
base_config_path = "./../../../kernel/configs/"
d_baseconfig = {}
d_sprdconfig = {}
d_defconfig = {}
d_diffconfig = {}
d_corrected_config = {}
d_all_plat = {}
not_defined = 0
config_path = "Documentation/sprd-configs.txt"
list_configs = []
tool_name = sys.argv[0][2:-3]
kernel_version = ''
all_arch = []
all_plat = []
l_sprdconfig = []
l_defproject = []
l_diffconfig = []
l_platform = [] # [p,q]

d_defconfig_path = {
        'kernel4.4':{
            'pike2':{'defconfig':'arch/arm/configs/sprd_pike2_defconfig', 'diffconfig':'sprd-diffconfig/pike2', 'arch':'arm'},
            'sharkle32':{'defconfig':'arch/arm/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm'},
            'sharkl3':{'defconfig':'arch/arm64/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/sharkl3', 'arch':'arm64'},
            'sharkle':{'defconfig':'arch/arm64/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm64'},
            'sharkle32_fp':{'defconfig':'arch/arm/configs/sprd_sharkle_fp_defconfig', 'diffconfig':'sprd-diffconfig/sharkle', 'arch':'arm'},
            'sharkl3_32':{'defconfig':'arch/arm/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/sharkl3', 'arch':'arm'},
        },
        'kernel4.14':{
            'pike2':{'defconfig':'arch/arm/configs/sprd_pike2_defconfig', 'diffconfig':'sprd-diffconfig/androidq/pike2', 'arch':'arm','platform':'q'},
            'roc1':{'defconfig':'arch/arm64/configs/sprd_roc1_defconfig', 'diffconfig':'sprd-diffconfig/androidq/roc1','arch':'arm64','platform':'p'},
            'sharkl3':{'defconfig':'arch/arm64/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/androidq/sharkl3','arch':'arm64','platform':'q'},
            'sharkl3_32':{'defconfig':'arch/arm/configs/sprd_sharkl3_defconfig', 'diffconfig':'sprd-diffconfig/androidq/sharkl3', 'arch':'arm','platform':'q'},
            'sharkl5':{'defconfig':'arch/arm64/configs/sprd_sharkl5_defconfig', 'diffconfig':'sprd-diffconfig/androidq/sharkl5','arch':'arm64','platform':'p'},
            'sharkl5_32':{'defconfig':'arch/arm/configs/sprd_sharkl5_defconfig', 'diffconfig':'sprd-diffconfig/androidq/sharkl5','arch':'arm','platform':'p'},
            'sharkle32':{'defconfig':'arch/arm/configs/sprd_sharkle_defconfig', 'diffconfig':'sprd-diffconfig/androidq/sharkle', 'arch':'arm','platform':'q'},
            'sharkl5Pro':{'defconfig':'arch/arm64/configs/sprd_sharkl5Pro_defconfig', 'diffconfig':'sprd-diffconfig/androidq/sharkl5Pro', 'arch':'arm64','platform':'q'},
        },
}

def create_baseconfig_dict():
# **** d_baseconfig[platform][arch][config] = 'y' ****
    for platform in l_platform:
        d_baseconfig[platform] = {}
        dpath = os.path.join(base_config_path,platform,kernel_version_plus)
        os.path.abspath(dpath)

        for maindir,subdir,filename_list in os.walk(dpath):
            for filename in filename_list:
                if "android-base" in filename:
                    arch = filename.split('.').pop(0).split('-').pop()
                    d_baseconfig[platform][arch] = {}

                    fpath = os.path.join(maindir,filename)
                    f = open(fpath,'r')
                    lines = f.readlines()
                    for j in range(len(lines)):
                        if '=' in lines[j]:
                            config_name = lines[j].split('=')[0]
                            d_baseconfig[platform][arch][config_name] = 'y'
                        elif 'is not set' in lines[j]:
                            config_name = lines[j].split(' ')[1]
                            d_baseconfig[platform][arch][config_name] = 'n'
                    f.close()

def check_base_vs_defconfig():
    for plat in d_defconfig_path[kernel_version]:
        platform = d_defconfig_path[kernel_version][plat]['platform']
        arch_base = d_defconfig_path[kernel_version][plat]['arch']
        print("\tBEGIN check " + plat + " vs " + platform + "_base\n")

# **** check base_arm/arm64.config(if exists) vs plat(arm/arm64)
        if arch_base in d_baseconfig[platform]:
            for config in d_baseconfig[platform][arch_base]:
                if config not in d_defconfig[plat]:
                    if d_baseconfig[platform][arch_base][config] == 'n':
                        print("WARNING: missing configuration: base:" + config + " " + d_baseconfig[platform][arch_base][config])
                    else:
                        print("ERROR: missing configuration: base:" + config + " " + d_baseconfig[platform][arch_base][config])
                elif config in d_defconfig[plat]:
                    if d_baseconfig[platform][arch_base][config] != d_defconfig[plat][config]:
                        print("ERROR: different configuration: base:" + config + " " + d_baseconfig[platform][arch_base][config] + "\t"\
                            + plat + ":" + config + " " + d_defconfig[plat][config])
# **** check base.config vs plat(no matter what arch)
        for config in d_baseconfig[platform]['base']:
            if config not in d_defconfig[plat]:
                if d_baseconfig[platform]['base'][config] == 'n':
                    print("WARNING: missing configuration: base:" + config + " " + d_baseconfig[platform]['base'][config])
                else:
                    print("ERROR: missing configuration: base:" + config + " " + d_baseconfig[platform][arch_base][config])
            elif config in d_defconfig[plat]:
                if d_baseconfig[platform]['base'][config] != d_defconfig[plat][config]:
                    print("ERROR: different configuration: base:" + config + " " + d_baseconfig[platform]['base'][config] + "\t"\
                        + plat + ":" + config + " " + d_defconfig[plat][config])

        print("\n\tEND check " + plat + "\n")

def create_diffconfigs_dict():
    result = []

    for key in d_defconfig_path[kernel_version]:
        for maindir,subdir,file_name_list in os.walk(d_defconfig_path[kernel_version][key]['diffconfig']):
            """
            print("1:",maindir) #current dir
            print("2:",subdir) # all subdirs
            print("3:",file_name_list)  #all subfiles
            """
            for filename in file_name_list:
                apath = os.path.join(maindir, filename)
                result.append(apath)

                f = open(apath,'r')
                lines = f.readlines()
                for j in range(len(lines)):
                    if 'ADD:' == lines[j][:4] or 'MOD:' == lines[j][:4]:
                        tmp_arch = apath.split("/").pop(3)
                        tmp_plat = apath.split("/").pop(2)

                        if tmp_arch == 'arm' and tmp_plat == 'sharkle':
                            file_name = apath.split("/").pop()
                            if 'mocor5' in file_name or 'kaios' in file_name:
                                tmp_plat = 'sharkle32_fp'
                            else:
                                tmp_plat = 'sharkle32'
                        elif tmp_arch == 'arm64' and tmp_plat == 'sharkle':
                            if kernel_version == 'kernel4.14':
                                tmp_arch = ""
                                tmp_plat = ""
                        elif tmp_arch == 'arm' and tmp_plat == 'sharkl3':
                            tmp_plat = 'sharkl3_32'
                        elif tmp_plat == 'pike2':
                            tmp_arch = 'arm'
                        elif tmp_arch == 'arm' and tmp_plat == 'sharkl5':
                            tmp_plat = 'sharkl5_32'
                        elif tmp_arch == 'common' and tmp_plat == 'sharkle':
                            if kernel_version == 'kernel4.4':
                                tmp_arch = 'arm,arm64'
                                tmp_plat = 'sharkle,sharkle32'
                            elif kernel_version == 'kernel4.14':
                                tmp_arch = 'arm'
                                tmp_plat = 'sharkle32'
                        elif tmp_arch == 'common' and tmp_plat == 'sharkl3':
                                tmp_arch = 'arm,arm64'
                                tmp_plat = 'sharkl3,sharkl3_32'
                        elif tmp_arch == 'common' and tmp_plat == 'sharkl5':
                            tmp_arch = 'arm,arm64'
                            tmp_plat = 'sharkl5,sharkl5_32'
                        elif tmp_arch == 'common' and tmp_plat == 'roc1':
                            tmp_arch = 'arm64'
                        elif tmp_arch == 'common' and tmp_plat == 'sharkl5Pro':
                            tmp_arch = 'arm64'

                        if lines[j][4:-1] in d_diffconfig:
                            if tmp_arch not in d_diffconfig[lines[j][4:-1]]['arch'].split(','):
                                d_diffconfig[lines[j][4:-1]]['arch'] = d_diffconfig[lines[j][4:-1]]['arch'] + tmp_arch + ','
                            if tmp_plat not in d_diffconfig[lines[j][4:-1]]['plat'].split(','):
                                d_diffconfig[lines[j][4:-1]]['plat'] = d_diffconfig[lines[j][4:-1]]['plat'] + tmp_plat + ','
                        else:
                            d_diffconfig[lines[j][4:-1]] = {
                                'arch': tmp_arch + ',',
                                'plat': tmp_plat + ',',
                                'field':'',
                                'subsys':'',
                                'must':'',
                                'function':''
                                }
                f.close

def create_sprdconfigs_dict():

    global d_sprdconfig

    f_sprdconfig = open(config_path,'r')
    lines = f_sprdconfig.readlines()
    values_of_config = ['[arch]','[plat]','[missing plat]','[field]','[subsys]','[must]','[missing plat description]','[function]']
    for i in range(len(lines)):
        if lines[i][:7] == "CONFIG_":
            config_name = lines[i][:-1]
            d_sprdconfig[config_name] = {'arch':'','plat':'','missing plat':'','field':'','subsys':'','must':'','missing plat description':'','function':''}
            num_lines_in_config = 0
            values_exist = []
            for n in range(i+1,len(lines)):
                num_lines_in_config += 1
                if lines[n][:7] == "CONFIG_":
                    break
                if n == len(lines)-1:
                    num_lines_in_config += 1
                    break

            for j in range(1,num_lines_in_config):
                for value in values_of_config:
                    value_sprd = value.split('[').pop().split(']').pop(0)
                    if value in lines[i+j]:
                        if value not in values_exist:
                            values_exist.append(value)
                            d_sprdconfig[config_name][value_sprd] = lines[i+j].split(value).pop().strip(' \n')
                        elif value in values_exist:
                            if d_sprdconfig[config_name][value_sprd] == '':
                                if len(lines[i+j]) > len(value):
                                    d_sprdconfig[config_name][value_sprd] = lines[i+j].split(value).pop().strip(' \n')
                        for m in range(j+1,num_lines_in_config):
                            if '[' in lines[i+m] and ']' in lines[i+m]:
                                break
                            if lines[i+m].strip() == '':
                                continue
                            d_sprdconfig[config_name][value_sprd] += '\n'+lines[i+m][:-1]
                        j = m
                        break
            i = i + num_lines_in_config
    f_sprdconfig.close()

    # create the d_sprdconfig by Documentation/sprdconfigs.txt
def print_incomplete_info(file_name):
    print('All configs           : ',len(d_sprdconfig))
    print('Not completed configs : ',not_defined)
    if not_defined > 0:
        print('File {} has been updated, Please Check.'.format(file_name))

def incomplete_item():
    global not_defined

    file_name = os.path.join(tmp_path,'need_completed.txt')
    if os.path.exists(file_name):
        os.remove(file_name)
    f_need_completed = open(file_name, 'a+')
    tmp_list = list(d_sprdconfig)
    tmp_list.sort()
    for key in tmp_list:
        if d_sprdconfig[key]['subsys'].strip() == "" or d_sprdconfig[key]['function'].strip() == "" or \
            d_sprdconfig[key]['field'].strip() == "" or d_sprdconfig[key]['must'].strip() == "" or \
            d_sprdconfig[key]['arch'].strip() == "" or d_sprdconfig[key]['plat'].strip() == "":
            not_defined += 1
            f_need_completed.write(key+'\n')
    f_need_completed.close()
    print_incomplete_info(file_name)

def configs_resort():
    list_configs = list(d_sprdconfig)
    list_configs.sort()
    os.remove(config_path)
    f=open(config_path,'a')
    for line in list_configs:
        # if plat = '', It should be deleted.
        if d_sprdconfig[line]['plat'] == '':
            continue
        f.write(line + '\n')
        if d_sprdconfig[line]['arch'] == '':
            f.write("\t[arch]\n")
        else:
            f.write("\t[arch] {}\n".format(d_sprdconfig[line]['arch']))

        if d_sprdconfig[line]['plat'] == '':
            f.write("\t[plat]\n")
        else:
            f.write("\t[plat] {}\n".format(d_sprdconfig[line]['plat']))

        if d_sprdconfig[line]['missing plat'] == 'none':
            f.write("\t[missing plat] none\n")
        else:
            f.write("\t[missing plat] {}".format(d_sprdconfig[line]['missing plat']).strip(' ') + '\n')

        if d_sprdconfig[line]['field'] == '':
            f.write("\t[field]\n")
        else:
            f.write("\t[field] {}\n".format(d_sprdconfig[line]['field']))

        if d_sprdconfig[line]['subsys'] == '':
            f.write("\t[subsys]\n")
        else:
            f.write("\t[subsys] {}\n".format(d_sprdconfig[line]['subsys']))

        if d_sprdconfig[line]['must'] == '':
            f.write("\t[must]\n")
        else:
            f.write("\t[must] {}\n".format(d_sprdconfig[line]['must']))

        if d_sprdconfig[line]['missing plat description'] == 'none':
            f.write("\t[missing plat description] none\n")
        else:
            f.write("\t[missing plat description] {}".format(d_sprdconfig[line]['missing plat description']).strip(' ') + '\n')

        if d_sprdconfig[line]['function'] == '':
            f.write("\t[function]\n")
        else:
            f.write("\t[function] {}\n".format(d_sprdconfig[line]['function']))

    f.close()

#d_defconfig={'project_name':{config_name:y/n},}
def create_defconfig_dict():
    """
    create each defconfig dict for each project.
    sys.argv[0] is .py
    sys.argv[1] is check
    """
    for key in d_defconfig_path[kernel_version]:
        d_defconfig[key] = {}
        path = d_defconfig_path[kernel_version][key]['defconfig']
        f_defconfig = open(path)
        lines = f_defconfig.readlines()

        for j in range(len(lines)):
            if '=' in lines[j]:
                config_name = lines[j].split('=')[0]
                d_defconfig[key][config_name] = 'y'
            elif 'is not set' in lines[j]:
                config_name = lines[j].split(' ')[1]
                d_defconfig[key][config_name] = 'n'
        f_defconfig.close()

#print all config defined in Documentation/sprd-configs.txt
def output_allconfigs():
    file_name = os.path.join(tmp_path,'all_sprdconfigs_status.txt')
    if os.path.exists(file_name):
        os.remove(file_name)
    f = open(file_name,'a')

    for key_config in l_corrected_config:
        f.write(key_config + ' :\t' + d_corrected_config[key_config]['plat'] + "\n")
    f.close()

def help_info():
        print(
        """
        SCRIPT_NAME : sprd_check-config_check.py
        DESCRIPTION : This script must be executed in the kernel root directory. This script is mainly used to check and update the ./Documentation/sprdconfigs.txt,
                      it also provides some other functions, which are all defined by the Python3 standard.
        USAGE       : script [option]
        EXAMPLE     : ./script/sprd/sprd_check-config_check.py update
        OPTIONS     :
                sort          : Resort the sprd-configs.txt by CONFIG in alphabetical order.

                incomplete    : Check the sprd-configs.txt incompleted config and output to need_completed.txt.

                check         : Check the defconfig and diffconfig, if there is any new defconfig(key=y) or diffconfig, then merge it into sprdconfig
                                and output all configs's project_status to allconfigs_status.txt.

                update        : Update only [CONFIG_NAME],[arch],[plat] of sprd-configs.txt now.

                allconfigs    : Output allconfigs of sprd-configs.txt to all_sprdconfigs.txt.

                aiaiai        : Find out the changes of defconfigs and diffconfigs, the diffences between CODE with DOC from lastest.diff, then print
                                the guide information.

                support       : Print all archs and plats supported.

                scan          : Scan all configs to export a statistical file named 'config_plat_scan.csv', which include Config_name, Enable_archs,
                                Enable_plats, ARM_missing_plats and ARM64_missing_plats.

                help          : Print the help information.
        """
        )

def create_corrected_dict():
    global d_corrected_config
    for key_defproject in l_defproject:
        l_defconfig = list(d_defconfig[key_defproject])
        l_defconfig.sort()
        for key_defconfig in l_defconfig:
            if key_defconfig not in d_corrected_config:
                if d_defconfig[key_defproject][key_defconfig] == 'y':
                    d_corrected_config[key_defconfig] = {'arch':'','plat':''}

    for key_diffconfig in l_diffconfig:
        if key_diffconfig in d_corrected_config:
            continue
        d_corrected_config[key_diffconfig] = {'arch':'','plat':''}

    global l_corrected_config
    l_corrected_config = list(d_corrected_config)
    l_corrected_config.sort()

    for key in l_corrected_config:
        tmp_arch = ''
        tmp_plat = ''
        for project in l_defproject:
            if key in d_defconfig[project]:
                if d_defconfig[project][key] == 'y':
                    tmp_plat = tmp_plat + project + ','
                else:
                    continue

                if d_defconfig_path[kernel_version][project]['arch'] not in tmp_arch.split(','):
                    tmp_arch = tmp_arch + d_defconfig_path[kernel_version][project]['arch'] + ','

        #TODO Doesn't check diffconfig
        if key in d_diffconfig:
            if d_diffconfig[key]['arch'] not in tmp_arch:
                tmp_arch = tmp_arch + d_diffconfig[key]['arch'] + ","

            if d_diffconfig[key]['plat'] not in tmp_plat:
                tmp_plat = tmp_plat + d_diffconfig[key]['plat'] + ","

        l_tmp_arch = tmp_arch[:-1].split(",")
        l_tmp_arch.sort()
        tmp_arch_sort = ''
        for i in range(len(l_tmp_arch)):
            if l_tmp_arch[i] not in tmp_arch_sort.split(","):
                tmp_arch_sort = tmp_arch_sort + l_tmp_arch[i] + ','
        tmp_plat_sort = ''
        l_tmp_plat = tmp_plat[:-1].split(",")
        l_tmp_plat.sort()
        for i in range(len(l_tmp_plat)):
            if l_tmp_plat[i] not in tmp_plat_sort.split(","):
                tmp_plat_sort = tmp_plat_sort + l_tmp_plat[i] + ','

        tmp_arch = tmp_arch_sort
        tmp_plat = tmp_plat_sort

        d_corrected_config[key] = { 'arch':'','plat':''}
        #write current status to dict d_sprdconfig
        if len(tmp_arch[:-1].split(",")) == len(all_arch):
            d_corrected_config[key]['arch'] = 'all'
        else:
            d_corrected_config[key]['arch'] = tmp_arch[:-1]

        if len(tmp_plat[:-1].split(",")) == len(all_plat):
            d_corrected_config[key]['plat'] = 'all'
        else:
            d_corrected_config[key]['plat'] = tmp_plat[:-1]

def ai_check_missing_plat():
     for key in d_corrected_config:
        corrected_missing_plat = ''
        if d_corrected_config[key]['arch'] == 'all':
            if d_corrected_config[key]['plat'] == 'all':
                corrected_missing_plat = ''
            else:
                for plat in all_plat:
                    if plat not in d_corrected_config[key]['plat'].split(","):
                        corrected_missing_plat = corrected_missing_plat + plat + ","

        elif d_corrected_config[key]['plat'] != d_all_plat[d_corrected_config[key]['arch']][:-1]:
            for plat in d_all_plat[d_corrected_config[key]['arch']].split(',')[:-1]:
                if plat not in d_corrected_config[key]['plat'].split(','):
                    corrected_missing_plat = corrected_missing_plat + plat + ','

        if corrected_missing_plat != '':
            l_doc_missing_plat = re.split(',| ',d_sprdconfig[key]['missing plat'])
            l_doc_missing_plat.sort()

            l_code_missing_plat = corrected_missing_plat[:-1].split(",")
            l_code_missing_plat.sort()

            if l_doc_missing_plat != l_code_missing_plat:
                print("ERROR: doc: Value is different between code and sprd-configs.txt. " + \
                        " CONFIG:" + key + \
                        " CODE:[missing plat]:" + missing_plat[:-1] + \
                        " DOC:[missing plat]:" + d_sprdconfig[key]['missing plat'])
            if d_sprdconfig[key]['missing plat description'] == 'none':
                print("ERROR: doc: " + key + " : [missing plat description] couldn't be none" \
                        + " and should be modified to describe reason of missing plat.")
        else:
            if d_sprdconfig[key]['missing plat'] != 'none' or d_sprdconfig[key]['missing plat description'] != 'none':
                print("ERROR: doc: " + key + " in sprd-configs.txt : [missing plat] and [missing plat description] should both be none, please check.")

def ai_check_incomplete():
     tmp_list = list(d_sprdconfig)
     tmp_list.sort()
     for key in tmp_list:
         if d_sprdconfig[key]['subsys'].strip() == "" or d_sprdconfig[key]['function'].strip() == "" or \
             d_sprdconfig[key]['field'].strip() == "" or d_sprdconfig[key]['must'].strip() == "" or \
             d_sprdconfig[key]['arch'].strip() == "" or d_sprdconfig[key]['plat'].strip() == "" or \
             d_sprdconfig[key]['missing plat'].strip() == "" or d_sprdconfig[key]['missing plat description'] == "":
             print("ERROR: doc: " + key + ": Anyone of the options in sprd-configs.txt can not be empty, please check.")

def aiaiai_check():
    print("========BEGIN========")
    file_name = tmp_path_def+"lastest.diff"
    os.system("git show HEAD -1 > " + file_name)

    f_diff = open(file_name, 'r')
    f_diff_lines = f_diff.readlines()
    ai_check_flag = 0
    for i in range(len(f_diff_lines)):
        if "diff --git" in f_diff_lines[i]:
            if "sprd-diffconfig"  or "sprd-configs.txt" or "defconfig" in f_diff_lines[i]:
                ai_check_flag = 1

    if ai_check_flag == 1:
        for key in d_sprdconfig:
            if key in d_corrected_config:
                continue
            else:
                print("ERROR: del: Need delete " + key + " from Documentation/sprd-configs.txt. " )

        for key in d_corrected_config:
            if key not in d_sprdconfig:
                print("ERROR: add: Need create new item: " + key + " to Documentation/sprd-configs.txt. ")
            else:
                if d_corrected_config[key]['arch'] != d_sprdconfig[key]['arch']:
                    print("ERROR: doc: Value is different between code and sprd-configs.txt. " + \
                            " CONFIG:" + key + \
                            " CODE:[arch]:" + d_corrected_config[key]['arch'] + \
                            " DOC:[arch]:" + d_sprdconfig[key]['arch'])
                if d_corrected_config[key]['plat'] != d_sprdconfig[key]['plat']:
                    plat_num_in_both = 0
                    plat_num_in_sprd = len(d_sprdconfig[key]['plat'].strip(' ,;.').split(','))
                    error_occur = 0

                    for plat in d_corrected_config[key]['plat'].split(','):
                        if plat not in d_sprdconfig[key]['plat'].split(','):
                            error_occur = 1
                            break
                        else:
                            plat_num_in_both += 1
                            continue

                    if plat_num_in_both != plat_num_in_sprd:
                        error_occur = 1

                    if error_occur == 1:
                        print("ERROR: doc: Value is different between code and sprd-configs.txt." + \
                                " CONFIG:" + key + \
                                " CODE:[plat]:" + d_corrected_config[key]['plat'] + \
                                " DOC:[plat]:" + d_sprdconfig[key]['plat'])
                else:
                    continue
        ai_check_missing_plat()
        ai_check_incomplete()
    f_diff.close()
    print("=========END=========")

def clean():
    os.system("rm -rf " + tmp_path_def)

def print_support_arch_plat():
    print("Current kernel information\n[arch]:{}\n[plat]:{}".format(all_arch,all_plat))

def update_sprd_configs():

    print_support_arch_plat()

    # write current status to dict d_sprdconfig
    d_del_sprdconfig = {}
    for key in d_corrected_config:
        if key not in d_sprdconfig:
            d_sprdconfig[key] = {'arch':'','plat':'','missing plat':'','field':'','subsys':'','must':'','missing plat description':'','function':''}
    for key in d_sprdconfig:
        if key not in d_corrected_config:
            d_del_sprdconfig[key] = d_sprdconfig[key]
        else:
            d_sprdconfig[key]['arch'] = d_corrected_config[key]['arch']
            d_sprdconfig[key]['plat'] = d_corrected_config[key]['plat']
    for key in d_del_sprdconfig:
        del d_sprdconfig[key]

    update_missing_plat()

    # regenerate sprd-configs.txt with dict d_sprdconfig
    configs_resort()
    print("\n\tsprd-configs.txt has been updated now")

def update_missing_plat():
    for key in d_sprdconfig:
        missing_plat = ''
        if d_sprdconfig[key]['arch'] == 'all':
            if d_sprdconfig[key]['plat'] == 'all':
                missing_plat = ''
            else:
                for plat in all_plat:
                    if plat not in d_sprdconfig[key]['plat'].split(","):
                        missing_plat = missing_plat + plat + ","

        elif d_sprdconfig[key]['plat'] != d_all_plat[d_sprdconfig[key]['arch']][:-1]:
            for plat in d_all_plat[d_sprdconfig[key]['arch']].split(',')[:-1]:
                if plat not in d_sprdconfig[key]['plat'].split(','):
                    missing_plat = missing_plat + plat + ','
        d_sprdconfig[key]['missing plat'] = missing_plat[:-1]

    for key in d_sprdconfig:
        if d_sprdconfig[key]['arch'] == 'all':
            if d_sprdconfig[key]['plat'] == 'all':
                d_sprdconfig[key]['missing plat'] = 'none'
                d_sprdconfig[key]['missing plat description'] = 'none'
        elif d_sprdconfig[key]['plat'] == d_all_plat[d_sprdconfig[key]['arch']][:-1]:
            d_sprdconfig[key]['missing plat'] = 'none'
            d_sprdconfig[key]['missing plat description'] = 'none'

        if d_sprdconfig[key]['missing plat'] != 'none':
            l_missing_plat = re.split(',| ',d_sprdconfig[key]['missing plat'])
            l_missing_plat.sort()
            for plat in l_missing_plat:
                if plat not in all_plat:
                    l_missing_plat.remove(plat)
            d_sprdconfig[key]['missing plat'] = ','.join(l_missing_plat).strip(' ,')

def special_scan():
    csv_filename = os.path.join(tmp_path,"unreasonable_missing_scan.csv")
    if os.path.exists(csv_filename):
        os.remove(csv_filename)
    csv_fd = open(csv_filename, 'a+')
    csv_writer = csv.writer(csv_fd)
    csv_writer.writerow(["Config name", "Enalbe arch", "Current enable plats","Current_missing_plat","Reasonable_missing_plat","Missing_plat_description"])
    for key in l_sprdconfig:
        l_write = []
        missing_plat = ''
        if d_sprdconfig[key]['arch'] == 'all':
            if d_sprdconfig[key]['plat'] == 'all':
                missing_plat = ''
            else:
                for plat in all_plat:
                    if plat not in d_sprdconfig[key]['plat'].split(","):
                        missing_plat = missing_plat + plat + ","

        elif d_sprdconfig[key]['plat'] != d_all_plat[d_sprdconfig[key]['arch']][:-1]:
            for plat in d_all_plat[d_sprdconfig[key]['arch']].split(',')[:-1]:
                if plat not in d_sprdconfig[key]['plat'].split(','):
                    missing_plat = missing_plat + plat + ','

        if missing_plat != '':
            l_write_flag = 0
            if d_sprdconfig[key]['missing plat'] != missing_plat[:-1]:
                l_write_flag = 1
            elif d_sprdconfig[key]['missing plat description'] == '' or d_sprdconfig[key]['missing plat description'] == 'none':
                l_write_flag = 1
            if l_write_flag == 1:
                l_write = [key, d_sprdconfig[key]['arch'], d_sprdconfig[key]['plat'], missing_plat[:-1], d_sprdconfig[key]['missing plat'], \
                            d_sprdconfig[key]['missing plat description']]
                csv_writer.writerow(l_write)
    csv_fd.close()

def scan():
    l_corrected_config = list(d_corrected_config)
    l_corrected_config.sort()

    str_all_arm_plat = ''
    str_all_arm64_plat = ''

    for key_project in l_defproject:
        if d_defconfig_path[kernel_version][key_project]['arch'] == 'arm':
            str_all_arm_plat = str_all_arm_plat + key_project + ','
        elif d_defconfig_path[kernel_version][key_project]['arch'] == 'arm64':
            str_all_arm64_plat = str_all_arm64_plat + key_project + ','

    str_all_arm_plat = str_all_arm_plat[:-1]
    str_all_arm64_plat = str_all_arm64_plat[:-1]

    csv_filename=os.path.join(tmp_path,"all_missing_plat_scan.csv")
    if os.path.exists(csv_filename):
        os.remove(csv_filename)
    csv_fd = open(csv_filename, 'a+')
    csv_writer = csv.writer(csv_fd)
    csv_writer.writerow(["Config name", "Enalbe arch", "Current enable plats", "ARM missing plat", "ARM64 missing plat"])
    for key_config in l_corrected_config:
        for key_arch in range(len(d_corrected_config[key_config]['arch'].split(","))):
            str_missing_arm = ""
            str_missing_arm64 = ""
            l_write = []
            if d_corrected_config[key_config]['arch'] == 'all':
                if d_corrected_config[key_config]['plat'] == 'all':
                    continue
                for i in range(len(str_all_arm_plat.split(","))):
                    if str_all_arm_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_missing_arm = str_missing_arm + str_all_arm_plat.split(",").pop(i) + ","
                        continue
                for i in range(len(str_all_arm64_plat.split(","))):
                    if str_all_arm64_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_missing_arm64 = str_missing_arm64 + str_all_arm64_plat.split(",").pop(i) + ","
                        continue
                if str_missing_arm != "" or str_missing_arm64 != "":
                    l_write = [key_config, d_corrected_config[key_config]['arch'], d_corrected_config[key_config]['plat'], str_missing_arm[:-1], str_missing_arm64[:-1]]
                    csv_writer.writerow(l_write)
            elif d_corrected_config[key_config]['arch'] == 'arm':
                for i in range(len(str_all_arm_plat.split(","))):
                    if str_all_arm_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_missing_arm = str_missing_arm + str_all_arm_plat.split(",").pop(i) + ","
                        continue
                if str_missing_arm != "":
                    l_write = [key_config, d_corrected_config[key_config]['arch'], d_corrected_config[key_config]['plat'], str_missing_arm[:-1], ""]
                    csv_writer.writerow(l_write)
            elif d_corrected_config[key_config]['arch'] == 'arm64':
                for i in range(len(str_all_arm64_plat.split(","))):
                    if str_all_arm64_plat.split(",").pop(i) not in d_corrected_config[key_config]['plat'].split(","):
                        str_missing_arm64 = str_missing_arm64 + str_all_arm64_plat.split(",").pop(i) + ","
                        continue
                if str_missing_arm64 != "":
                    l_write = [key_config, d_corrected_config[key_config]['arch'], d_corrected_config[key_config]['plat'], "", str_missing_arm64[:-1]]
                    csv_writer.writerow(l_write)
    csv_fd.close()

def prepare_info_first():
    f = open("Makefile", 'r')
    lines = f.readlines()
    for j in range(3):
        if 'VERSION' in lines[j]:
            version = lines[j].split(" ").pop(2)
        if 'PATCHLEVEL' in lines[j]:
            patchlevel = lines[j].split(" ").pop(2)
    f.close
    global kernel_version
    kernel_version = 'kernel' + version[:-1] + '.' + patchlevel[:-1]
    global kernel_version_plus
    kernel_version_plus = 'android-' +version[:-1] + '.' + patchlevel[:-1]

    l_defconfig_path = list(d_defconfig_path[kernel_version])
    l_defconfig_path.sort()

    for key in l_defconfig_path:
        if d_defconfig_path[kernel_version][key]['platform'] not in l_platform:
            l_platform.append(d_defconfig_path[kernel_version][key]['platform'])

        all_plat.append(key)
        if d_defconfig_path[kernel_version][key]['arch'] not in all_arch:
            all_arch.append(d_defconfig_path[kernel_version][key]['arch'])

        d_all_plat.setdefault(d_defconfig_path[kernel_version][key]['arch'],'')
        if key not in d_all_plat[d_defconfig_path[kernel_version][key]['arch']]:
             d_all_plat[d_defconfig_path[kernel_version][key]['arch']] = d_all_plat[d_defconfig_path[kernel_version][key]['arch']] + key + ","

def print_script_info():
    print(
    """
    If sprd_check-config_check error, please using script to update Documentation/sprd-configs.txt.
    The step as following:
    1)  Enter kernel root path. (kernel4.4 / kernel4.14)
    2)  Using script to update [arch] & [plat] automatically which in Documentation/sprd-configs.txt.
        Command: ./scripts/sprd/sprd_check-config_check.py update
    3)  Check the Documentation/sprd-configs.txt. Please don't modify the [arch] & [plat].
        The others such as [field] [subsys] [must] [sound] need to fill in by owner.
        The sprd_configs.txt introduction refer to Documentation/sprd_configs_introduction.txt
    """)

def prepare_info_second():
    global l_sprdconfig
    l_sprdconfig = list(d_sprdconfig)
    l_sprdconfig.sort()

    global l_defproject
    l_defproject = list(d_defconfig)
    l_defproject.sort()

    global l_diffconfig
    l_diffconfig = list(d_diffconfig)
    l_diffconfig.sort()

def main():
    global tmp_path

    tmp_path = tmp_path_def
    folder = os.path.exists(tmp_path)
    if not folder:
        os.makedirs(tmp_path)

    prepare_info_first()
    create_defconfig_dict()
    create_diffconfigs_dict()
    create_sprdconfigs_dict()
    prepare_info_second()
    create_corrected_dict()
    create_baseconfig_dict()

    if len(sys.argv) == 2:
        if sys.argv[1] == 'sort':
            configs_resort()
            print("The sprd-configs.txt has been resorted.")
        elif sys.argv[1] == 'help':
            help_info()
        elif sys.argv[1] == 'aiaiai':
            print_script_info()
            aiaiai_check()
            clean()
        elif sys.argv[1] == 'update':
            update_sprd_configs()
            clean()
        elif sys.argv[1] == 'support':
            print_support_arch_plat()
        elif sys.argv[1] == 'allconfigs':
            output_allconfigs()
        elif sys.argv[1] == 'incomplete':
            incomplete_item()
        elif sys.argv[1] == 'scan':
            scan()
            special_scan()
        elif sys.argv[1] == 'checkbase':
            check_base_vs_defconfig()
        else:
            print("PARAMETERS ERROR:")
            help_info()
    elif len(sys.argv)  == 3:
        tmp_path=sys.argv[2]
        folder = os.path.exists(tmp_path)
        if not folder:
            os.makedirs(tmp_path)

        if sys.argv[1] == 'allconfigs':
            output_allconfigs()
        elif sys.argv[1] == 'incomplete':
            incomplete_item()
        elif sys.argv[1] == 'scan':
            scan()
            special_scan()
        else:
            print("PARAMETERS ERROR:")
            help_info()
            os.system("rmdir " + tmp_path)
    else:
        print("PARAMETERS ERROR:")
        help_info()

if __name__ == '__main__':
    main()

