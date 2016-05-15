/*
	Copyright (C) 2009-2013 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "types.h"
#include "task.h"
#include <stdio.h>

#include <rthreads/rthreads.h>

class Task::Impl {
private:
	sthread_t *_thread;
	bool _isThreadRunning;
	
public:
	Impl();
	~Impl();

	void start(void);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();

   slock_t *mutex;
   scond_t *condWork;
	TWork workFunc;
	void *workFuncParam;
	void *ret;
	bool exitThread;
};

static void taskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;

	do {
		slock_lock(ctx->mutex);

		while (!ctx->workFunc && !ctx->exitThread)
			scond_wait(ctx->condWork, ctx->mutex);

		if (ctx->workFunc)
			ctx->ret = ctx->workFunc(ctx->workFuncParam);
      else
			ctx->ret = NULL;

		ctx->workFunc = NULL;
		scond_signal(ctx->condWork);

		slock_unlock(ctx->mutex);

	} while(!ctx->exitThread);
}

Task::Impl::Impl()
{
	_isThreadRunning = false;
	workFunc         = NULL;
	workFuncParam    = NULL;
	ret              = NULL;
	exitThread       = false;

	mutex            = slock_new();
   condWork         = scond_new();
}

Task::Impl::~Impl()
{
	shutdown();
	slock_free(mutex);
	scond_free(condWork);
}

void Task::Impl::start(void)
{
	slock_lock(this->mutex);

	if (this->_isThreadRunning)
   {
		slock_unlock(this->mutex);
		return;
	}

	this->workFunc         = NULL;
	this->workFuncParam    = NULL;
	this->ret              = NULL;
	this->exitThread       = false;
	this->_isThreadRunning = true;
	this->_thread          = (sthread_t*)sthread_create(&taskProc, this);

	slock_unlock(this->mutex);
}

void Task::Impl::execute(const TWork &work, void *param)
{
	slock_lock(this->mutex);

	if (!work || !this->_isThreadRunning)
      goto end;

	this->workFunc      = work;
	this->workFuncParam = param;
	scond_signal(this->condWork);

end:
	slock_unlock(this->mutex);
}

void* Task::Impl::finish()
{
	void *returnValue = NULL;

	slock_lock(this->mutex);

	if (!this->_isThreadRunning)
      goto end;

	while (this->workFunc != NULL)
		scond_wait(this->condWork, this->mutex);

	returnValue = this->ret;

end:
	slock_unlock(this->mutex);

	return returnValue;
}

void Task::Impl::shutdown()
{
	slock_lock(this->mutex);

	if (!this->_isThreadRunning)
      goto end;

	this->workFunc   = NULL;
	this->exitThread = true;
	scond_signal(this->condWork);

	slock_unlock(this->mutex);
	sthread_join(this->_thread);

	slock_lock(this->mutex);
	this->_isThreadRunning = false;

end:
	slock_unlock(this->mutex);
}

void Task::start(void) { impl->start(); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


