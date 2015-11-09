%Context = type opaque

declare i32 @puts(i8* nocapture) nounwind
declare void @fpbind(%Context*) nounwind
declare void @fpload(%Context*) nounwind
declare void @fpgoto_table(%Context*) nounwind

@.hello = private unnamed_addr constant [13 x i8] c"hello world\0A\00"


define void @process(%Context* %c)
{
	%1 = getelementptr [13 x i8]* @.hello, i32 0, i32 0
    call i32 @puts(i8* %1)

    call void @fpbind(%Context* %c)
    call void @fpload(%Context* %c)
    call void @fpgoto_table(%Context* %c)

    ret void
}