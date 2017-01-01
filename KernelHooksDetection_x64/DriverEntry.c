#include "peload.h"
#include <ntddk.h>  
#include "LDE64x64.h"
#include "KernelCheck.h"



#define DEVICE_NAME L"\\device\\ntmodeldrv"  
#define LINK_NAME L"\\dosdevices\\ntmodeldrv"  

#define IOCTRL_BASE 0x800  

#define MYIOCTRL_CODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTRL_BASE + i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define CTL_HELLO MYIOCTRL_CODE(0)  
#define CTL_PRINT MYIOCTRL_CODE(1)  
#define CTL_BYE MYIOCTRL_CODE(2)  

NTSTATUS DispatchCommon(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DispatchCreate(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DispatchRead(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	PVOID pReadBuffer = NULL;
	ULONG uReadLength = 0;
	PIO_STACK_LOCATION pStack = NULL;
	ULONG uMin = 0;
	ULONG uHelloStr = 0;

	uHelloStr = (wcslen(L"hello world") + 1) * sizeof(WCHAR);

	pReadBuffer = pIrp->AssociatedIrp.SystemBuffer;
	pStack = IoGetCurrentIrpStackLocation(pIrp);

	uReadLength = pStack->Parameters.Read.Length;
	uMin = uReadLength>uHelloStr ? uHelloStr : uReadLength;

	RtlCopyMemory(pReadBuffer, L"hello world", uMin);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = uMin;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;

}

NTSTATUS DispatchWrite(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	PVOID pWriteBuff = NULL;
	ULONG uWriteLength = 0;
	PIO_STACK_LOCATION pStack = NULL;

	PVOID pBuffer = NULL;

	pWriteBuff = pIrp->AssociatedIrp.SystemBuffer;

	pStack = IoGetCurrentIrpStackLocation(pIrp);
	uWriteLength = pStack->Parameters.Write.Length;

	pBuffer = ExAllocatePoolWithTag(PagedPool, uWriteLength, 'TSET');
	if (pBuffer == NULL)
	{
		pIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	memset(pBuffer, 0, uWriteLength);

	RtlCopyMemory(pBuffer, pWriteBuff, uWriteLength);

	ExFreePool(pBuffer);
	pBuffer = NULL;


	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = uWriteLength;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;

}

NTSTATUS DispatchIoctrl(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	ULONG uIoctrlCode = 0;
	PVOID pInputBuff = NULL;
	PVOID pOutputBuff = NULL;

	ULONG uInputLength = 0;
	ULONG uOutputLength = 0;
	PIO_STACK_LOCATION pStack = NULL;

	pInputBuff = pOutputBuff = pIrp->AssociatedIrp.SystemBuffer;

	pStack = IoGetCurrentIrpStackLocation(pIrp);
	uInputLength = pStack->Parameters.DeviceIoControl.InputBufferLength;
	uOutputLength = pStack->Parameters.DeviceIoControl.OutputBufferLength;


	uIoctrlCode = pStack->Parameters.DeviceIoControl.IoControlCode;

	switch (uIoctrlCode)
	{
	case CTL_HELLO:
		DbgPrint("Hello iocontrol\n");
		break;
	case CTL_PRINT:
		DbgPrint("%ws\n", pInputBuff);
		break;
	case CTL_BYE:
		DbgPrint("Goodbye iocontrol\n");
		break;
	default:
		DbgPrint("Unknown iocontrol\n");

	}

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;

}

NTSTATUS DispatchClean(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DispatchClose(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}


VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING uLinkName = { 0 };
	ExFreePool(LDE);
	RtlInitUnicodeString(&uLinkName, LINK_NAME);
	IoDeleteSymbolicLink(&uLinkName);

	IoDeleteDevice(pDriverObject->DeviceObject);

	DbgPrint("Driver unloaded\n");

}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegPath)
{
	UNICODE_STRING uDeviceName = { 0 };
	UNICODE_STRING uLinkName = { 0 };
	NTSTATUS ntStatus = 0;
	PDEVICE_OBJECT pDeviceObject = NULL;
	ULONG i = 0;
	PDRIVER_INFO pDriverInfo = NULL;
	CHAR		*szDriverName = NULL;

	g_DriverObject = pDriverObject;

	if (!GetSystemKernelModuleInfo(pDriverObject, &g_SystemKernelFilePath, &g_SystemKernelModuleBase, &g_SystemKernelModuleSize, TRUE, NULL))
		return STATUS_UNSUCCESSFUL;

	DbgPrint("Driver load begin\n");
	szDriverName = ExAllocatePool(NonPagedPool, MAX_PATH * 2);
	if (szDriverName == NULL)
		return STATUS_UNSUCCESSFUL;
	g_pDrvPatchInfo = ExAllocatePool(NonPagedPool, sizeof(DRIVER_PATCH_INFO) * 1000);
	if (g_pDrvPatchInfo == NULL)
	{
		ExFreePool(szDriverName);
		return STATUS_UNSUCCESSFUL;
	}
	RtlZeroMemory(g_pDrvPatchInfo, sizeof(DRIVER_PATCH_INFO) * 1000);

	RtlInitUnicodeString(&uDeviceName, DEVICE_NAME);
	RtlInitUnicodeString(&uLinkName, LINK_NAME);

	ntStatus = IoCreateDevice(pDriverObject,
		0, &uDeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("IoCreateDevice failed:%x", ntStatus);
		return ntStatus;
	}

	pDeviceObject->Flags |= DO_BUFFERED_IO;

	ntStatus = IoCreateSymbolicLink(&uLinkName, &uDeviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		IoDeleteDevice(pDeviceObject);
		DbgPrint("IoCreateSymbolicLink failed:%x\n", ntStatus);
		return ntStatus;
	}

	for (i = 0; i<IRP_MJ_MAXIMUM_FUNCTION + 1; i++)
	{
		pDriverObject->MajorFunction[i] = DispatchCommon;
	}

	pDriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
	pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = DispatchWrite;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctrl;
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = DispatchClean;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;

	pDriverObject->DriverUnload = DriverUnload;

	DbgPrint("Driver load ok!\n");

	// KeServiceDescriptorTableShadow �ﱣ����SSDT ��SSSDT�����������Ϣ ��KeServiceDescriptorTableֻ��SSDT����Ϣ
	KeServiceDescriptorTable = (PSYSTEM_SERVICE_TABLE)GetKeServiceDescriptorTableShadow64();
	KeServiceDescriptorTableShadow = (PSYSTEM_SERVICE_TABLE)((ULONG64)KeServiceDescriptorTable + sizeof(SYSTEM_SERVICE_TABLE));
	
	DbgPrint(
		"KeServiceDescriptorTable:0x%p\r\n"
		"KeServiceDescriptorTableShadow 0x%p\r\n"
		"KeServiceDescriptorTable->ServiceTableBase:0x%p\r\n"
		"KeServiceDescriptorTableShadow->ServiceTableBase:0x%p\r\n"
		,KeServiceDescriptorTable
		,KeServiceDescriptorTableShadow
		,KeServiceDescriptorTable->ServiceTableBase
		,KeServiceDescriptorTableShadow->ServiceTableBase
	);

	LDE_init();

	/*CheckTest(L"\\SystemRoot\\system32\\win32k.sys", "win32k.sys");

	CheckTest(L"\\SystemRoot\\system32\\ntoskrnl.exe", "ntoskrnl.exe");*/
	i = 0;
	RtlZeroMemory(szDriverName, MAX_PATH*2);
	if (EnumDriver(pDriverObject, &pDriverInfo))
	{
		i = pDriverInfo[0].Count;
		for (size_t k = 0; k < i; k++)
		{
			WcharToChar(pDriverInfo[k].DriverName, szDriverName);

			CheckTest(pDriverInfo[k].DriverPath, szDriverName, pDriverInfo[k].DriverName);
			RtlZeroMemory(szDriverName, MAX_PATH * 2);
		}

		DbgPrint("Patch Count:%d\r\n", g_uPatchCount);
		if (g_uPatchCount >= 500)
		{
			DbgPrint("Patch Count too long ! Check faild !\n");
			goto Exit;
		}

		for (size_t j = 0; j < g_uPatchCount; j++)
		{
			DbgPrint(
				"PatchAddress:0x%p\r\n"
				"DriverName:%ws\r\n"
				"DriverPath:%ws\r\n"
				,g_pDrvPatchInfo[j].PathchAddr
				,g_pDrvPatchInfo[j].DriverName
				,g_pDrvPatchInfo[j].DriverPath
			);

			PrintBytes("CurrentCode:", g_pDrvPatchInfo[j].CurrentCode, 15);
			PrintBytes("OrigCode:", g_pDrvPatchInfo[j].OrigCode, 15);
		}
	}
	Exit:
	ExFreePool(g_pDrvPatchInfo);
	g_pDrvPatchInfo = NULL;
	ExFreePool(szDriverName);
	szDriverName = NULL;
	if(pDriverInfo)
		ExFreePool(pDriverInfo);

	pDriverInfo = NULL;

	return STATUS_SUCCESS;
}