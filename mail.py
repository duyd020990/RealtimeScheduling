import sys
import smtplib
from email.mime.text import MIMEText
from email.header import Header

mail_host = 'smtp.163.com'
mail_user = '15754866960@163.com'
mail_pass = 'lt943210'

sender = '15754866960@163.com'
receivers = ['linaqin@jaist.ac.jp']

def send_mail(file_name):

	f = open(file_name,'r')
	mail_content = f.read()
	
	message = MIMEText(mail_content,'plain','utf-8')
	message['From'] = Header('Experiment')
	message['To'] = Header('linaqin@jaist.ac.jp')
	
	subject = 'Exeperiment Result'
	message['Subject'] = Header(subject,'utf-8')
	
	try:
		smtpObj = smtplib.SMTP()
		smtpObj.connect(mail_host,25)
		smtpObj.login(mail_user,mail_pass)
		smtpObj.sendmail(sender,receivers,message.as_string())
		print('Experiment result send success')
	except smtplib.SMTPException:
		print('Error')

if __name__ == '__main__':
	arg_len = len(sys.argv)
	if arg_len < 2:
		send_mail('results.csv')
	else:
		argv = sys.argv
		send_mail(argv[1])