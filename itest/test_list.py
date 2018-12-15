import asyncio
import functools
import random
import time
from testing import Client
from testing import default_test_setup
from testing import gen_data
from testing import gen_points
from testing import gen_series
from testing import InsertError
from testing import PoolError
from testing import QueryError
from testing import run_test
from testing import Series
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError
from testing import parse_args


TIME_PRECISION = 's'


class TestList(TestBase):
    title = 'Test list'

    GEN_POINTS = functools.partial(
        gen_points, n=1, time_precision=TIME_PRECISION)

    @default_test_setup(1, time_precision=TIME_PRECISION)
    async def run(self):
        await self.client0.connect()

        # Create some random series and start 25 insert task parallel
        series = gen_series(n=10000)
        tasks = [
            asyncio.ensure_future(
                self.client0.insert_some_series(
                    series,
                    timeout=0,
                    points=self.GEN_POINTS))
            for i in range(25)]

        await asyncio.gather(*tasks)

        await self.client0.query('list series /.*/ - /.*/')
        await self.client0.query('list series /.*/ | /.*/')
        await self.client0.query('list series /.*/ & /.*/')
        await self.client0.query('list series /.*/ ^ /.*/')

        await self.client0.query('list series /.*/ - /a.*/')
        await self.client0.query('list series /.*/ | /a.*/')
        await self.client0.query('list series /.*/ & /a.*/')
        await self.client0.query('list series /.*/ ^ /a.*/')

        await self.client0.query('list series /.*/ - /a.*/ | /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ | /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ | /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ | /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ | /.*/')
        await self.client0.query('list series /.*/ | /a.*/ | /.*/')
        await self.client0.query('list series /.*/ & /a.*/ | /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ | /.*/')

        await self.client0.query('list series /.*/ - /a.*/ - /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ - /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ - /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ - /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ - /.*/')
        await self.client0.query('list series /.*/ | /a.*/ - /.*/')
        await self.client0.query('list series /.*/ & /a.*/ - /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ - /.*/')

        await self.client0.query('list series /.*/ - /a.*/ & /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ & /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ & /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ & /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ & /.*/')
        await self.client0.query('list series /.*/ | /a.*/ & /.*/')
        await self.client0.query('list series /.*/ & /a.*/ & /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ & /.*/')

        await self.client0.query('list series /.*/ - /a.*/ ^ /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ ^ /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ ^ /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ ^ /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ ^ /.*/')
        await self.client0.query('list series /.*/ | /a.*/ ^ /.*/')
        await self.client0.query('list series /.*/ & /a.*/ ^ /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ ^ /.*/')

        await self.client0.query('alter database set list_limit 5000')
        with self.assertRaisesRegex(
                QueryError,
                'Limit must be a value between 0 and 5000 '
                'but received: 6000.*'):
            await self.client0.query(
                'list series limit 6000')

        self.client0.close()

        # return False


if __name__ == '__main__':
    parse_args()
    run_test(TestList())
