#!/usr/bin/python

import sys, getopt, os, pprint, ast

original_results_dir = ''
saved_results_dir = ''

def main(argv):
   #read the arguments
   try:
      opts, args = getopt.getopt(argv,"ho:s:",["original=","saved="])
   except getopt.GetoptError:
      print ('compare-ooxml-analyze-results.py -o <original results> -s <saved results>')
      sys.exit(2)

   for opt, arg in opts:
      if opt == '-h':
         print ('compare-ooxml-analyze-results.py -o <original results> -s <saved results>')
         sys.exit()
      elif opt in ("-o", "--original"):
         global original_results_dir
         original_results_dir = arg
      elif opt in ("-s", "--saved"):
         global saved_results_dir
         saved_results_dir = arg

   # takes file list produced by ooxml-analyze.py tool.
   original_result_files = get_list_of_files(original_results_dir)
   saved_result_files = get_list_of_files(saved_results_dir)

   compare(original_result_files, saved_result_files)

def get_list_of_files(directory_name):

   list_of_file = os.listdir(directory_name)
   all_files = list()

   for filename in list_of_file:
      full_path = os.path.join(directory_name, filename)
      if os.path.isdir(full_path):
         all_files = all_files + get_list_of_files(full_path)
      else:
         all_files.append(full_path)

   return all_files


# compares the  elements the original results and and after saved results.
def compare(original_result_files, saved_result_files):
   ind = 1
   for original_filepath in original_result_files:
      saved_filepath = get_corresponding_file(original_filepath)
      if saved_filepath == '':
         print("No result file after roundtrip for " + original_filepath)
         continue

      original_result_list = create_list_from_result_file(original_filepath)
      saved_result_list = create_list_from_result_file(saved_filepath)

      check_text_contents(original_result_list, saved_result_list)


# checks if we missed any text content after saving the file.
def check_text_contents(original_result_list, saved_result_list):

   # detect if we lost or added any text on existing texts of original version.
   for line in original_result_list:
      text_dict = line[3]
      if not bool(text_dict): # check if text context is empty
         continue
      tag = list(line[0].keys())[0] #if there is a text context, find the owner tag.
      for sline in saved_result_list:
         stag = list(sline[0].keys())[0]
         if stag == tag: # check if saved results has same tag too.
            saved_text_dict = sline[3]
            if text_dict != saved_text_dict:
               for key, val in text_dict.items():
                  if key not in saved_text_dict.keys():
                     print ("We lost %d \"%s\" text in %s tag." % (val, key, tag))
                  elif val > saved_text_dict[key]:
                     print ("We lost %d \"%s\" text in %s tag." % (val - saved_text_dict[key], key, tag))
                  elif val < saved_text_dict[key]:
                     print("We added extra %d \"%s\" text in %s tag" % (saved_text_dict[key] - val, key, tag))

   # detect if we add any new text that not existed in original version
   for line in saved_result_list:
      saved_text_dict = line[3]
      if not bool(saved_text_dict): # check if text context is empty
         continue
      tag = list(line[0].keys())[0] #if there is a text context, find the owner tag.
      for sline in original_result_list:
         stag = list(sline[0].keys())[0]
         if stag == tag: # check if original results has same tag too.
            text_dict = sline[3]
            if saved_text_dict != text_dict:
               for key, val in saved_text_dict.items():
                  if key not in text_dict.keys():
                     print ("We add extra %d \"%s\" text in %s tag." % (val, key, tag))

#reads the file context and create the result list structer from.
# eg res_list[[{},{},{},{}],[{},{},{},{}]...]                                                                 ]
def create_list_from_result_file(filepath):
   result_list = []
   result_file = open(filepath, 'r')

   for line in result_file.readlines():
      tmp_list = [{}, {}, {}, {}]

      i = line.find('{')
      j = line.find('},')
      tmp_list[0] = ast.literal_eval(line[i:j+1])

      line = line[j+1:]

      i = line.find('{')
      j = line.find('},')
      tmp_list[1] = ast.literal_eval(line[i:j+1])

      line = line[j+1:]

      i = line.find('{')
      j = line.find('},')
      tmp_list[2] = ast.literal_eval(line[i:j+1])

      line = line[j+1:]

      i = line.find('{')
      j = line.find('}]')
      tmp_list[3] = ast.literal_eval(line[i:j+1])

      result_list.append(tmp_list)
   return result_list

# takes the original result file and returns corresponding saved one's path
def get_corresponding_file(filepath):
   i = filepath.rfind('/')
   filename = filepath[i+1:]
   saved_filepath = os.path.join(saved_results_dir, filename)

   if(os.path.exists(saved_filepath)):
      return saved_filepath
   return ''

if __name__ == "__main__":
    main(sys.argv[1:])
