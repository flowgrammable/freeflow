; Simple wire application.

%Context = type opaque
@.proc_msg = private unnamed_addr constant [11 x i8] c"processed\0A\00"
@.conf_msg = private unnamed_addr constant [12 x i8] c"configured\0A\00"
declare i32 @puts(i8* nocapture) nounwind

define void @process(%Context* %cxt)
{
  call i32 @puts(i8* getelementptr [11 x i8]* @.proc_msg, i32 0, i32 0)
}

define void @config(void)
{
  call i32 @puts(i8* getelementptr [12 x i8]* @.conf_msg, i32 0, i32 0)
}

define void @port(i32* in)
{
  store i32* in, i32 2
}
