#!/usr/bin/python
# -*- coding: UTF-8 -*-

import os
import sys
import commands

MAIN_PATH = ''
TAGS_FILE_NAME = '../../Documentation/sprd-tags.txt'

GET_PATCH_INFO_COMMANDS = 'git log -1'
GET_PATCH_MODIFY_FILE_INFO = 'git log -1 --pretty="format:" --name-only --diff-filter=AM'

ATTRIBUTE_TAGS  = []
SUBSYSTEM1_TAGS = []
SUBSYSTEM2_TAGS = []
SUBSYSTEM3_TAGS = []
SUBSYSTEM1_TAGS_NOCHECK = []
SPECIAL_CHECK_TAGS = ['Documentation', 'dts']
check_tags_flag = 1

def read_line(path, file_name):
    read_file = path + '/' + file_name
    f = open(read_file, 'rb')
    lines = f.readlines()
    f.close()
    return lines

def get_tags():
    global MAIN_PATH
    global ATTRIBUTE_TAGS
    global SUBSYSTEM1_TAGS
    global SUBSYSTEM2_TAGS
    global SUBSYSTEM3_TAGS
    global SUBSYSTEM1_TAGS_NOCHECK
    get_tags_flag = 0

    MAIN_PATH = os.path.dirname(os.path.abspath(sys.argv[0]))

#print "main path: %s" % MAIN_PATH

    read_tags_list = read_line(MAIN_PATH, TAGS_FILE_NAME)

    for x in read_tags_list:
        if "\n" in x:
            x = x.strip("\n")

        if "[info]" in x and get_tags_flag == 0:
            get_tags_flag = 1
            continue
        elif get_tags_flag == 0:
            continue

        if ":" in x and get_tags_flag == 1:
            subsystem1_tags = ''
            subsystem2_tags = ''
            subsystem3_tags = ''

            subsystem_list = x.split(":")
#print "subsystem tags:%s %d" % (subsystem_list,len(subsystem_list))
            if '*' in x:
                SUBSYSTEM1_TAGS_NOCHECK.append(subsystem_list[0].replace(' ',''))
                continue

            subsystem1_tags = subsystem_list[0].replace(' ','')
            if len(subsystem_list) >= 4:
                subsystem2_tags = subsystem_list[1].replace(' ','')
                subsystem3_tags = subsystem_list[2].replace(' ','')
            elif len(subsystem_list) >= 3:
                subsystem2_tags = subsystem_list[1].replace(' ','')

#print "subsystem tags: 1:%s,2:%s,3:%s" % (subsystem1_tags,subsystem2_tags,subsystem3_tags)

            if subsystem1_tags not in SUBSYSTEM1_TAGS:
                SUBSYSTEM1_TAGS.append(subsystem1_tags)
                SUBSYSTEM2_TAGS.append([subsystem2_tags])
                SUBSYSTEM3_TAGS.append([subsystem3_tags])
            else:
                if len(subsystem_list) >= 4:
                    if subsystem2_tags not in SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)]:
                        SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)].append(subsystem2_tags)
                    if subsystem3_tags not in SUBSYSTEM3_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)]:
                        SUBSYSTEM3_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)].append(subsystem3_tags)
                elif len(subsystem_list) >= 3:
                    if subsystem2_tags not in SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)]:
                        SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)].append(subsystem2_tags)

#print "SUBSYSTEM1_TAGS: %s" % SUBSYSTEM1_TAGS
#print "SUBSYSTEM2_TAGS: %s" % SUBSYSTEM2_TAGS
#print "SUBSYSTEM3_TAGS: %s" % SUBSYSTEM3_TAGS

        elif "," in x and get_tags_flag == 1:
            ATTRIBUTE_TAGS = x.split(",")
#           print "attribute tags:%s" % ATTRIBUTE_TAGS

#    print "SUBSYSTEM1_TAGS_NOCHECK: %s" % SUBSYSTEM1_TAGS_NOCHECK
#    print "SUBSYSTEM1_TAGS: %s" % SUBSYSTEM1_TAGS
#    print "SUBSYSTEM1_TAGS num = %d" % len(SUBSYSTEM1_TAGS)

def find_last_char(string, p):
    index = 0
    i = 0
    for x in string:
        if p == x:
            index = i
        i += 1

    return index

def check_tags_commit_id(patch_info_list):
    global check_tags_flag
    check_title_flag = 1
    check_commit_id_flag = 0
    tags_list_start_num = 0
    ret_hit_tags_list = []
    attribute_temp = []

    get_tags()

    for x in patch_info_list:
        if check_title_flag == 1 and "Bug #" in x:
            print("Patch title:\n%s" % x)
            check_title_flag = 0

            if "：" in x:
                return (-1, "The patch title contains : of chinese")
            if ":" not in x:
                return (-1, "The patch donot contains tag")

            if len(x.split(":")) != len(x.split(": ")):
                return (-1, "expected ' ' after ':'")

            tags_list = x[x.index("Bug #") + len("Bug #"):find_last_char(x, ":")].split(' ')[1:]
#            print "tags list:%s" % tags_list

            if "BACKPORT" == tags_list[tags_list_start_num]:
                tags_list_start_num += 1

            if tags_list[tags_list_start_num].strip(":") in ATTRIBUTE_TAGS:
                if tags_list[tags_list_start_num].strip(":") in ATTRIBUTE_TAGS[0:ATTRIBUTE_TAGS.index("FROMLIST") + 1]:
                    check_tags_flag = 0
                    check_commit_id_flag = 0
                elif tags_list[tags_list_start_num].strip(":") in  ATTRIBUTE_TAGS[ATTRIBUTE_TAGS.index("SECURITY"):]:
                    check_tags_flag = 1
                else:
                    check_tags_flag = 0
                    check_commit_id_flag = 1
                tags_list_start_num += 1

            if check_tags_flag == 1:
                if tags_list_start_num < len(tags_list):
                    if tags_list[tags_list_start_num].strip(":") in SUBSYSTEM1_TAGS_NOCHECK:
                        ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                   elif tags_list[tags_list_start_num].strip(":") in SUBSYSTEM1_TAGS:
                        ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                        tags_list_start_num += 1
                        if tags_list_start_num < len(tags_list):
                            if tags_list[tags_list_start_num].strip(":") in SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(tags_list[tags_list_start_num - 1].strip(":"))]:
                                ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                                tags_list_start_num += 1
                                if tags_list_start_num < len(tags_list):
                                    if tags_list[tags_list_start_num].strip(":") in SUBSYSTEM3_TAGS[SUBSYSTEM1_TAGS.index(tags_list[tags_list_start_num - 2].strip(":"))]:
                                        ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                                        continue
                                    else:
                                        return (-1, "The subsystem 3 tag is error")
                                else:
                                    continue
                            else:
                                return (-1, "The subsystem 2 tag is error")
                        else:
                            continue
                    else:
                        return (-1, "The subsystem 1 tag is error")
                else:
                    return (-1, "The title donot contains subsystem tag")
        elif check_commit_id_flag == 1 and "commit" in x:
            # check commit id ok
            print("check commit id ok")
            return (0, ret_hit_tags_list)

    if check_commit_id_flag == 1:
        return (-1, "The patch donot contains commit id")

    return (0, ret_hit_tags_list)

def check_tags_file(modify_file_list, tags_list):
    file_name_list_temp = []
    inconsistent_file_list = []
    file_add_inconsistent_flag = 0
    file_and_tag_consistent_flag = 0
    special_inconsistent_flag = 0

    if "asoc" in tags_list:
        tags_list[tags_list.index("asoc")] = 'sound'

    for x in modify_file_list:
        file_add_inconsistent_flag = 0

        if len(x) < 2:
            continue
        elif "." in x:
            file_name_list_temp = x.split(".")[0].split("/")
        else:
            file_name_list_temp = x.split("/")

        print("Modified file name: %s" % x)

        for y in tags_list:
            if y not in file_name_list_temp:
                file_add_inconsistent_flag = 1
                for z in file_name_list_temp:
                    if z in y:
                        file_add_inconsistent_flag = 0
                        break
                    elif y in z:
                        file_add_inconsistent_flag = 0
                        break
                if file_add_inconsistent_flag == 1:
                    inconsistent_file_list.append(x)
                    for special_tag in SPECIAL_CHECK_TAGS:
                        if special_tag in tags_list:
                            special_inconsistent_flag = 1
                            break
                        if special_tag in file_name_list_temp:
                            special_inconsistent_flag = 1
                            break
                    break

        if file_add_inconsistent_flag == 0 and file_and_tag_consistent_flag == 0:
            file_and_tag_consistent_flag = 1

    if file_and_tag_consistent_flag == 1 and special_inconsistent_flag == 0:
        return (0, inconsistent_file_list)

    return (-1, inconsistent_file_list)

if __name__ == '__main__':
    ret_info = []
    ret_check_file = []
    get_patch_info_list = []
    get_patch_modify_file_list = []

    status,output=commands.getstatusoutput(GET_PATCH_INFO_COMMANDS)

    get_patch_info_list = output.split('\n')

#print "get patch info:"
#for x in get_patch_info_list:
#print "%s" % x

    ret_info = check_tags_commit_id(get_patch_info_list)
    if ret_info[0] != 0:
        print("\nERROR: %s" %  ret_info[1])
        print("Please read 'Documentation/sprd-tags.txt' file.")
    else:
        print("\nCheck tags OK, tags list: %s\n" % ret_info[1])

        if check_tags_flag == 1:
            status,output=commands.getstatusoutput(GET_PATCH_MODIFY_FILE_INFO)
            get_patch_modify_file_list = output.split('\n')

            ret_check_file = check_tags_file(get_patch_modify_file_list, ret_info[1])

            if ret_check_file[0] != 0:
                print("\nERROR: Tags and modified files are inconsistent!")
                print("Please read 'Documentation/sprd-tags.txt' file.\n\ninconsistent file list:")
            elif len(ret_check_file[1]) > 0:
                print("\nWARNING: Tags and modified files are inconsistent.\n\ninconsistent file list:")
            else:
                print("\nTags and modified files are consistent!")

            if len(ret_check_file[1]) > 0:
                for x in ret_check_file[1]:
                    print("%s" % x)
        else:
            print("\nINFO:Ignore check of tags and modified files!")

#test code
'''
if __name__ == '__main__':
    test_list = []
    ret_info = []
#test 1 tags
    test_list = []
    test_list.append("Bug #987917 sched: open eas,set schedtune and set default schedutil governor @samer.xie")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test 2 tags
    test_list = []
    test_list.append("e79b6a3eaac9 Bug #986255 spi: sprd: Add reset function for Sharkl3 platform @Bruce Chen")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test 3 tags
    test_list = []
    test_list.append("Bug #898471 soc: sprd: iommu: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test include tags
    test_list = []
    test_list.append("Bug #885833 include: power123: Add helper function to detect charger type @Baolin Wang")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test donot need check tags and commit id
    test_list = []
    test_list.append("Bug #898471 FROMLIST: soc: sprda: iommu: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test donot need check tags, need check commit id
    test_list = []
    test_list.append("Bug #898471 UPSTREAM: socs: sprd: iommu: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test donot need check commit id,need check tags
    test_list = []
    test_list.append("Bug #898471 SECURITY: soc: sprda: iommu: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test 1 tags error
    test_list = []
    test_list.append("Bug #898471 soca: sprd: iommu: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test 2 tags error
    test_list = []
    test_list.append("Bug #898471 soc: sprda: iommu: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test 3 tags error
    test_list = []
    test_list.append("Bug #898471 soc: sprd: iommua: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test BACKPORT, FROMLIST:
    test_list = []
    test_list.append("Bug #898471 BACKPORT, FROMLIST: soc: sprda: iommua: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
#test BACKPORT: FROMGIT:
    test_list = []
    test_list.append("Bug #898471 BACKPORT: FROMGIT: soc: sprda: iommua: bypass dcam/isp iommu for sharkl5 @sheng.xu")
    ret_info = check_tags_commit_id(test_list)
    if ret_info[0] != 0:
        print "error code: %s, error info: %s" % (ret_info[0], ret_info[1])
    else:
        print "check OK: tags list: %s" % ret_info[1]
'''
